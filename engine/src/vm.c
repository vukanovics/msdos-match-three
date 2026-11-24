#include "engine/vm.h"

#include <string.h>

// Bytecode format: magic "BC04", u16 slot count, u16 reserved, u32 init offset,
// u32 tick offset, u32 key offset, u32 animation-end offset, u32 code size, then
// the code. All values little endian.
#define HEADER_SIZE 32
#define OP_PUSH 0x01
#define OP_DUP 0x02
#define OP_DROP 0x03
#define OP_SWAP 0x04
#define OP_OVER 0x05
#define OP_ADD 0x10
#define OP_SUB 0x11
#define OP_MUL 0x12
#define OP_DIV 0x13
#define OP_NEG 0x14
#define OP_MIN 0x15
#define OP_MAX 0x16
#define OP_MOD 0x17
#define OP_FX_MUL 0x18
#define OP_FX_DIV 0x19
#define OP_EQ 0x20
#define OP_NE 0x21
#define OP_LT 0x22
#define OP_GT 0x23
#define OP_LE 0x24
#define OP_GE 0x25
#define OP_AND 0x28
#define OP_OR 0x29
#define OP_NOT 0x2a
#define OP_GET 0x30
#define OP_SET 0x31
#define OP_GET_INDEXED 0x32
#define OP_SET_INDEXED 0x33
#define OP_RANDOM 0x34
#define OP_QUOTE 0x40
#define OP_JUMP 0x41
#define OP_WHEN 0x43
#define OP_IFELSE 0x44
#define OP_RET 0x45
#define OP_HALT 0x46
#define OP_JZ 0x47

static uint16_t read_u16(const uint8_t *at) {
  return (uint16_t)(at[0] | ((uint16_t)at[1] << 8));
}

static uint32_t read_u32(const uint8_t *at) {
  return (uint32_t)at[0] | ((uint32_t)at[1] << 8) | ((uint32_t)at[2] << 16) |
         ((uint32_t)at[3] << 24);
}

static void push(vm *this, int32_t value) {
  if (this->stack_size >= VM_STACK_SIZE) {
    this->error = "stack overflow";
    return;
  }
  this->stack[this->stack_size++] = value;
}

static int32_t pop_operand(vm *this) {
  if (this->stack_size <= 0) {
    this->error = "stack underflow";
    return 0;
  }
  return this->stack[--this->stack_size];
}

static bool slot_index(vm *this, int32_t slot) {
  if (slot < 0 || slot >= (int32_t)this->slot_count) {
    this->error = "slot out of range";
    return false;
  }
  return true;
}

uint32_t vm_random_state(const vm *this) { return this->random_state; }

bool vm_load(vm *this, const uint8_t *bytes, size_t size, int32_t *slots,
             uint16_t slot_count, uint32_t fingerprint, uint32_t seed) {
  memset(this, 0, sizeof(*this));

  if (size < HEADER_SIZE || memcmp(bytes, "BC04", 4) != 0) {
    this->error = "not a behaviour program";
    return false;
  }

  uint16_t compiled_slots = read_u16(bytes + 4);
  uint32_t code_size = read_u32(bytes + 24);

  if (compiled_slots != slot_count || read_u32(bytes + 28) != fingerprint) {
    this->error = "behaviour was compiled for a different slot layout";
    return false;
  }
  if (code_size > size - HEADER_SIZE) {
    this->error = "behaviour program is truncated";
    return false;
  }

  this->code = bytes + HEADER_SIZE;
  this->code_size = code_size;
  this->init_entry = read_u32(bytes + 8);
  this->tick_entry = read_u32(bytes + 12);
  this->key_entry = read_u32(bytes + 16);
  this->animation_end_entry = read_u32(bytes + 20);
  this->slots = slots;
  this->slot_count = slot_count;
  this->random_state = seed ? seed : 1u;
  this->error = NULL;
  return true;
}

static bool run(vm *this, uint32_t entry, int32_t argument,
                bool has_argument) {
  if (this->error) {
    return false;
  }
  if (entry >= this->code_size) {
    this->error = "entry point is outside the program";
    return false;
  }

  this->stack_size = 0;
  this->return_depth = 0;
  if (has_argument) {
    push(this, argument);
  }

  uint32_t ip = entry;
  bool done = false;

  while (!this->error && !done) {
    if (ip >= this->code_size) {
      this->error = "ran past the end of the program";
      break;
    }
    uint8_t opcode = this->code[ip++];
    int32_t a;
    int32_t b;

    switch (opcode) {
    case OP_PUSH:
      if (ip + 4 > this->code_size) {
        this->error = "truncated push";
        break;
      }
      push(this, (int32_t)read_u32(this->code + ip));
      ip += 4;
      break;

    case OP_DUP:
      if (this->stack_size <= 0) {
        this->error = "stack underflow";
        break;
      }
      push(this, this->stack[this->stack_size - 1]);
      break;

    case OP_DROP:
      (void)pop_operand(this);
      break;

    case OP_SWAP:
      if (this->stack_size < 2) {
        this->error = "stack underflow";
        break;
      }
      a = this->stack[this->stack_size - 1];
      this->stack[this->stack_size - 1] = this->stack[this->stack_size - 2];
      this->stack[this->stack_size - 2] = a;
      break;

    case OP_OVER:
      if (this->stack_size < 2) {
        this->error = "stack underflow";
        break;
      }
      push(this, this->stack[this->stack_size - 2]);
      break;

    case OP_ADD:
    case OP_SUB:
    case OP_MUL:
    case OP_DIV:
    case OP_MIN:
    case OP_MAX:
    case OP_MOD:
    case OP_EQ:
    case OP_NE:
    case OP_LT:
    case OP_GT:
    case OP_LE:
    case OP_GE:
    case OP_AND:
    case OP_OR:
      b = pop_operand(this);
      a = pop_operand(this);
      if (this->error) {
        break;
      }
      switch (opcode) {
      case OP_ADD:
        push(this, a + b);
        break;
      case OP_SUB:
        push(this, a - b);
        break;
      case OP_MUL:
        push(this, a * b);
        break;
      case OP_DIV:
        if (b == 0) {
          this->error = "division by zero";
          break;
        }
        push(this, a / b);
        break;
      case OP_MIN:
        push(this, a < b ? a : b);
        break;
      case OP_MAX:
        push(this, a > b ? a : b);
        break;
      case OP_MOD:
        if (b == 0) {
          this->error = "division by zero";
          break;
        }
        push(this, a % b);
        break;
      case OP_EQ:
        push(this, a == b);
        break;
      case OP_NE:
        push(this, a != b);
        break;
      case OP_LT:
        push(this, a < b);
        break;
      case OP_GT:
        push(this, a > b);
        break;
      case OP_LE:
        push(this, a <= b);
        break;
      case OP_GE:
        push(this, a >= b);
        break;
      case OP_AND:
        push(this, a != 0 && b != 0);
        break;
      default:
        push(this, a != 0 || b != 0);
        break;
      }
      break;

    case OP_FX_MUL:
      // Q16.16 by Q16.16, through a 64-bit intermediate: the product is Q32.32
      // before the shift, which does not fit in 32 bits.
      b = pop_operand(this);
      a = pop_operand(this);
      if (this->error) {
        break;
      }
      push(this, (int32_t)(((int64_t)a * (int64_t)b) >> 16));
      break;

    case OP_FX_DIV:
      b = pop_operand(this);
      a = pop_operand(this);
      if (this->error) {
        break;
      }
      if (b == 0) {
        this->error = "division by zero";
        break;
      }
      push(this, (int32_t)((((int64_t)a) << 16) / (int64_t)b));
      break;

    case OP_NEG:
      a = pop_operand(this);
      push(this, -a);
      break;

    case OP_NOT:
      a = pop_operand(this);
      push(this, a == 0);
      break;

    case OP_GET:
      if (ip + 2 > this->code_size) {
        this->error = "truncated slot access";
        break;
      }
      {
        int32_t slot = read_u16(this->code + ip);
        ip += 2;
        if (slot_index(this, slot)) {
          push(this, this->slots[slot]);
        }
      }
      break;

    case OP_SET:
      if (ip + 2 > this->code_size) {
        this->error = "truncated slot access";
        break;
      }
      {
        int32_t slot = read_u16(this->code + ip);
        ip += 2;
        a = pop_operand(this);
        if (!this->error && slot_index(this, slot)) {
          this->slots[slot] = a;
        }
      }
      break;

    case OP_GET_INDEXED: {
      int32_t slot = pop_operand(this);
      if (!this->error && slot_index(this, slot)) {
        push(this, this->slots[slot]);
      }
      break;
    }

    case OP_SET_INDEXED: {
      int32_t slot = pop_operand(this);
      a = pop_operand(this);
      if (!this->error && slot_index(this, slot)) {
        this->slots[slot] = a;
      }
      break;
    }

    case OP_RANDOM: {
      // xorshift32, masked to a non-negative value so 'mod' is never negative.
      uint32_t value = this->random_state;
      value ^= value << 13;
      value ^= value >> 17;
      value ^= value << 5;
      this->random_state = value;
      push(this, (int32_t)(value & 0x7fffffffu));
      break;
    }

    case OP_QUOTE:
      if (ip + 4 > this->code_size) {
        this->error = "truncated quotation";
        break;
      }
      push(this, (int32_t)read_u32(this->code + ip));
      ip += 4;
      break;

    case OP_JUMP:
      if (ip + 4 > this->code_size) {
        this->error = "truncated jump";
        break;
      }
      ip = read_u32(this->code + ip);
      break;

    case OP_JZ: {
      if (ip + 4 > this->code_size) {
        this->error = "truncated jump";
        break;
      }
      uint32_t target = read_u32(this->code + ip);
      ip += 4;
      a = pop_operand(this);
      if (!this->error && a == 0) {
        ip = target;
      }
      break;
    }

    case OP_WHEN: {
      int32_t quotation = pop_operand(this);
      int32_t condition = pop_operand(this);
      if (this->error) {
        break;
      }
      if (condition) {
        if (this->return_depth >= VM_CALL_DEPTH) {
          this->error = "call depth exceeded";
          break;
        }
        this->returns[this->return_depth++] = ip;
        ip = (uint32_t)quotation;
      }
      break;
    }

    case OP_IFELSE: {
      int32_t otherwise = pop_operand(this);
      int32_t then = pop_operand(this);
      int32_t condition = pop_operand(this);
      if (this->error) {
        break;
      }
      if (this->return_depth >= VM_CALL_DEPTH) {
        this->error = "call depth exceeded";
        break;
      }
      this->returns[this->return_depth++] = ip;
      ip = (uint32_t)(condition ? then : otherwise);
      break;
    }

    case OP_RET:
      if (this->return_depth <= 0) {
        // Falling off a quotation is not an error; treat it as the end of the
        // program so a malformed program cannot loop.
        done = true;
        break;
      }
      ip = this->returns[--this->return_depth];
      break;

    case OP_HALT:
      done = true;
      break;

    default:
      this->error = "unknown opcode";
      break;
    }
  }

  if (!this->error && this->stack_size != 0) {
    this->error = "stack not empty at return";
  }
  return !this->error;
}

bool vm_init(vm *this) { return run(this, this->init_entry, 0, false); }

bool vm_tick(vm *this) { return run(this, this->tick_entry, 0, false); }

bool vm_key(vm *this, int32_t key) {
  return run(this, this->key_entry, key, true);
}

bool vm_animation_end(vm *this, int32_t entity) {
  return run(this, this->animation_end_entry, entity, true);
}

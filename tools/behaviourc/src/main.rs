//! Compiler for the engine's behaviour language.
//!
//! The language is a total, non-recursive subset of Joy: concatenative, no
//! variables, quotations instead of closures. Definitions are inlined, so a
//! program cannot recurse and therefore always terminates. Values are 32-bit
//! integers; fixed point is the same representation with a convention.
//!
//! The program describes the world only through the scene's slot table, so the
//! compiler reads the same scene file the engine loads. Named slots are the
//! scene's globals; the entity pool is addressed arithmetically with '@' and
//! '!', the computed-index forms of the same operations.
//!
//! Usage: behaviourc <scene> <program> <output.bc>

use std::collections::HashMap;
use std::env;
use std::fs;
use std::process::exit;

const OP_PUSH: u8 = 0x01;
const OP_DUP: u8 = 0x02;
const OP_DROP: u8 = 0x03;
const OP_SWAP: u8 = 0x04;
const OP_OVER: u8 = 0x05;
const OP_ADD: u8 = 0x10;
const OP_SUB: u8 = 0x11;
const OP_MUL: u8 = 0x12;
const OP_DIV: u8 = 0x13;
const OP_NEG: u8 = 0x14;
const OP_MIN: u8 = 0x15;
const OP_MAX: u8 = 0x16;
const OP_MOD: u8 = 0x17;
const OP_FX_MUL: u8 = 0x18;
const OP_FX_DIV: u8 = 0x19;
const OP_EQ: u8 = 0x20;
const OP_NE: u8 = 0x21;
const OP_LT: u8 = 0x22;
const OP_GT: u8 = 0x23;
const OP_LE: u8 = 0x24;
const OP_GE: u8 = 0x25;
const OP_AND: u8 = 0x28;
const OP_OR: u8 = 0x29;
const OP_NOT: u8 = 0x2a;
const OP_GET: u8 = 0x30;
const OP_SET: u8 = 0x31;
const OP_GET_INDEXED: u8 = 0x32;
const OP_SET_INDEXED: u8 = 0x33;
const OP_RANDOM: u8 = 0x34;
const OP_QUOTE: u8 = 0x40;
const OP_JUMP: u8 = 0x41;
const OP_WHEN: u8 = 0x43;
const OP_IFELSE: u8 = 0x44;
const OP_RET: u8 = 0x45;
const OP_HALT: u8 = 0x46;
const OP_JZ: u8 = 0x47;

const MAGIC: &[u8; 4] = b"BC03";
// Must match ENTITY_STRIDE in engine/include/engine/slots.h: the compiler bakes
// the entity pool's size into every slot index it emits.
const ENTITY_STRIDE: i32 = 7;
const MAX_INLINE_DEPTH: usize = 32;

// 'for' is a counted loop whose count is a literal, which is what keeps it
// bounded. The limit is a sanity bound on the literal, not on code size.
const MAX_UNROLL: i32 = 1024;

fn fail(message: String) -> ! {
    eprintln!("behaviourc: {message}");
    exit(1);
}

struct Layout {
    entity_count: i32,
    names: Vec<String>,
    index: HashMap<String, u16>,
    // Sprite name -> handle, where a handle is the cel's index in the flat list
    // of every sheet the scene names, in declaration order.
    sprites: HashMap<String, u32>,
}

impl Layout {
    fn slot_count(&self) -> i32 {
        self.entity_count * ENTITY_STRIDE + self.names.len() as i32
    }
}

// Same rules as the engine's scene loader, because the offsets the compiler bakes
// in have to be the offsets the engine uses.
fn push_slot(names: &mut Vec<String>, name: &str) -> Result<(), String> {
    if name.starts_with('@') || name.starts_with('!') {
        return Err(format!("slot name '{name}' must not start with @ or !"));
    }
    if name.len() >= 32 {
        return Err(format!("slot name '{name}' is too long"));
    }
    if names.iter().any(|existing| existing == name) {
        return Err(format!("duplicate slot '{name}'"));
    }
    names.push(name.to_string());
    Ok(())
}

// Reads the same keys the engine's scene loader reads, so the offsets the
// compiler bakes in are the offsets the engine uses.
fn parse_scene(text: &str) -> Result<Layout, String> {
    let mut entity_count = None;
    let mut names: Vec<String> = Vec::new();
    let mut sheets: Vec<(String, i32)> = Vec::new();
    let mut sprites: Vec<(String, u32)> = Vec::new();

    for line in text.lines() {
        let line = line.split('#').next().unwrap_or("").trim();
        if line.is_empty() {
            continue;
        }
        let mut parts = line.split_whitespace();
        let key = parts.next().unwrap();

        if key == "entities.count" {
            let value: i32 = parts
                .next()
                .ok_or("entities.count needs a value")?
                .parse()
                .map_err(|_| "entities.count must be a whole number")?;
            if value < 0 || value > 256 {
                return Err(format!("entities.count {value} is out of range"));
            }
            entity_count = Some(value);
            continue;
        }

        if key == "sheet" {
            let name = parts.next().ok_or("'sheet' needs a name")?.to_string();
            let _path = parts.next().ok_or("'sheet' needs a file")?;
            let count: i32 = parts
                .next()
                .ok_or("'sheet' needs a cel count")?
                .parse()
                .map_err(|_| "'sheet' needs a whole number of cels")?;
            if count < 1 || count > 128 {
                return Err(format!("sheet '{name}' needs 1..128 cels"));
            }
            if sheets.len() >= 8 {
                return Err("more than 8 sheets".into());
            }
            if sheets.iter().any(|(existing, _)| existing == &name) {
                return Err(format!("duplicate sheet '{name}'"));
            }
            sheets.push((name, count));
            continue;
        }

        if key == "sprite" || key == "animatedsprite" {
            let animated = key == "animatedsprite";
            let name = parts.next().ok_or("'sprite' needs a name")?.to_string();
            let sheet_name = parts.next().ok_or("'sprite' needs a sheet")?.to_string();
            let cel: i32 = parts
                .next()
                .ok_or("'sprite' needs a cel")?
                .parse()
                .map_err(|_| "'sprite' needs a whole cel number")?;
            if name.starts_with('$') {
                return Err(format!(
                    "sprite name '{name}' must not start with $, which is how the \
                     behaviour program refers to it"
                ));
            }
            if sprites.iter().any(|(existing, _)| existing == &name) {
                return Err(format!("duplicate sprite '{name}'"));
            }
            // Only an animated sprite names a cel count and a tick rate; later
            // tokens say how it plays ('loop') and how it is loaded
            // ('resident'), and do not change the layout the compiler bakes in.
            let (frames, ticks): (i32, i32) = if animated {
                let frames = parts
                    .next()
                    .ok_or("'animatedsprite' needs a cel count")?
                    .parse()
                    .map_err(|_| "'animatedsprite' needs a whole cel count")?;
                let ticks = parts
                    .next()
                    .ok_or("'animatedsprite' needs a tick count")?
                    .parse()
                    .map_err(|_| "'animatedsprite' needs a whole tick count")?;
                (frames, ticks)
            } else {
                (1, 0)
            };
            if frames < 1 {
                return Err(format!("sprite '{name}' needs a cel count of 1 or more"));
            }
            if animated && ticks < 1 {
                return Err(format!("sprite '{name}' needs a tick count of 1 or more"));
            }
            let count = sheets
                .iter()
                .find(|(sheet, _)| sheet == &sheet_name)
                .map(|(_, count)| *count)
                .ok_or_else(|| {
                    format!(
                        "unknown sheet '{sheet_name}' (declare sheets before the \
                         sprites that use them)"
                    )
                })?;
            if cel < 0 || cel >= count {
                return Err(format!(
                    "sheet '{sheet_name}' has 0..{} cels, not {cel}",
                    count - 1
                ));
            }
            if cel + frames > count {
                return Err(format!(
                    "sprite '{name}' runs to cel {}, past the {count} cels of \
                     sheet '{sheet_name}'",
                    cel + frames
                ));
            }
            // A handle is the sprite's position in this table, not a cel
            // number, so the program never needs to know how cels are laid out.
            let handle = sprites.len() as u32;
            sprites.push((name, handle));
            continue;
        }

        if key == "array" {
            let name = parts.next().ok_or("'array' needs a name")?.to_string();
            let count: i32 = parts
                .next()
                .ok_or("'array' needs a count")?
                .parse()
                .map_err(|_| "'array' needs a whole number of elements")?;
            let initial = parts.next().ok_or("'array' needs a value")?;
            initial
                .parse::<i32>()
                .map_err(|_| format!("array '{name}' needs a whole number"))?;
            if count < 1 || count > 256 {
                return Err(format!("array '{name}' needs 1..256 elements"));
            }
            for index in 0..count {
                push_slot(&mut names, &format!("{name}{index}"))?;
            }
            continue;
        }

        if key != "slot" {
            continue;
        }

        let name = parts.next().ok_or("'slot' needs a name")?;
        let initial = parts.next().ok_or("'slot' needs a value")?;
        initial
            .parse::<i32>()
            .map_err(|_| format!("slot '{name}' needs a whole number"))?;
        push_slot(&mut names, name)?;
    }

    let entity_count = entity_count.ok_or("the scene has no 'entities.count'")?;
    let base = entity_count * ENTITY_STRIDE;

    let mut index = HashMap::new();
    for (position, name) in names.iter().enumerate() {
        let slot = base + position as i32;
        if slot > u16::MAX as i32 {
            return Err("too many slots".into());
        }
        index.insert(name.clone(), slot as u16);
    }

    let layout = Layout {
        entity_count,
        names,
        index,
        sprites: sprites.into_iter().collect(),
    };
    if layout.slot_count() > u16::MAX as i32 {
        return Err("too many slots".into());
    }
    Ok(layout)
}

struct Compiler<'a> {
    layout: &'a Layout,
    definitions: HashMap<String, Vec<String>>,
    code: Vec<u8>,
}

impl<'a> Compiler<'a> {
    fn new(layout: &'a Layout) -> Self {
        Compiler {
            layout,
            definitions: HashMap::new(),
            code: Vec::new(),
        }
    }

    fn emit(&mut self, byte: u8) {
        self.code.push(byte);
    }

    fn emit_u16(&mut self, value: u16) {
        self.code.extend_from_slice(&value.to_le_bytes());
    }

    fn emit_u32(&mut self, value: u32) {
        self.code.extend_from_slice(&value.to_le_bytes());
    }

    fn here(&self) -> usize {
        self.code.len()
    }

    fn patch_u32(&mut self, offset: usize, value: u32) {
        self.code[offset..offset + 4].copy_from_slice(&value.to_le_bytes());
    }

    fn compile_quotation(
        &mut self,
        tokens: &[String],
        at: &mut usize,
        depth: usize,
    ) -> Result<(), String> {
        // tokens[*at] is '['.
        *at += 1;
        let quote_at = self.here();
        self.emit(OP_QUOTE);
        self.emit_u32(0);
        let jump_at = self.here();
        self.emit(OP_JUMP);
        self.emit_u32(0);

        let body = self.here() as u32;
        self.patch_u32(quote_at + 1, body);

        while *at < tokens.len() {
            let token = tokens[*at].clone();
            if token == "]" {
                *at += 1;
                self.emit(OP_RET);
                let end = self.here() as u32;
                self.patch_u32(jump_at + 1, end);
                return Ok(());
            }
            self.compile_token(&token, tokens, at, depth)?;
        }

        Err("quotation is missing its closing ']'".into())
    }

    // 'for <count> [ ... ]' pushes 0..count-1 in turn and runs the body for
    // each, compiled straight through. The count must be a literal, which is
    // what keeps the unrolling bounded.
    fn compile_iteration(
        &mut self,
        tokens: &[String],
        at: &mut usize,
        depth: usize,
    ) -> Result<(), String> {
        *at += 1;
        let count_token = tokens.get(*at).ok_or("'for' needs a count")?.clone();
        *at += 1;
        let count: i32 = count_token
            .parse()
            .map_err(|_| "'for' needs a whole number of iterations".to_string())?;
        if count < 0 || count > MAX_UNROLL {
            return Err(format!(
                "'for' needs 0..{MAX_UNROLL} iterations, got {count}"
            ));
        }
        if tokens.get(*at).map(String::as_str) != Some("[") {
            return Err("'for' must be followed by a '[' quotation".into());
        }

        *at += 1;
        let start = *at;
        let mut end = start;
        let mut nesting = 0;
        while end < tokens.len() {
            match tokens[end].as_str() {
                "[" => nesting += 1,
                "]" => {
                    if nesting == 0 {
                        break;
                    }
                    nesting -= 1;
                }
                _ => {}
            }
            end += 1;
        }
        if end >= tokens.len() {
            return Err("'for' quotation is missing its closing ']'".into());
        }
        let body = tokens[start..end].to_vec();
        *at = end + 1;

        // A counted loop: seed the index, run the body, step, repeat. The count
        // is a literal so this still terminates, but the body is emitted once
        // rather than once per iteration - the difference between a hundred-cell
        // pass costing forty bytes and costing four kilobytes.
        self.emit(OP_PUSH);
        self.emit_u32(0);
        let loop_at = self.here() as u32;
        self.emit(OP_DUP);
        self.emit(OP_PUSH);
        self.emit_u32(count as u32);
        self.emit(OP_LT);
        let check_at = self.here();
        self.emit(OP_JZ);
        self.emit_u32(0);
        self.emit(OP_DUP);

        let mut inner = 0;
        while inner < body.len() {
            let token = body[inner].clone();
            self.compile_token(&token, &body, &mut inner, depth)?;
        }

        self.emit(OP_PUSH);
        self.emit_u32(1);
        self.emit(OP_ADD);
        self.emit(OP_JUMP);
        self.emit_u32(loop_at);

        // The loop exits with the index still on the stack; drop it here.
        self.patch_u32(check_at + 1, self.here() as u32);
        self.emit(OP_DROP);
        Ok(())
    }

    fn compile_token(
        &mut self,
        token: &str,
        tokens: &[String],
        at: &mut usize,
        depth: usize,
    ) -> Result<(), String> {
        if token == "[" {
            return self.compile_quotation(tokens, at, depth);
        }
        if token == "]" {
            return Err("unmatched ']'".into());
        }
        if token == "for" {
            return self.compile_iteration(tokens, at, depth);
        }
        *at += 1;

        if let Ok(value) = token.parse::<i32>() {
            self.emit(OP_PUSH);
            self.emit_u32(value as u32);
            return Ok(());
        }
        // Bare '@' and '!' take the slot index from the stack.
        if token == "@" {
            self.emit(OP_GET_INDEXED);
            return Ok(());
        }
        if token == "!" {
            self.emit(OP_SET_INDEXED);
            return Ok(());
        }
        // '$name' is the handle of a sprite the scene named.
        if let Some(name) = token.strip_prefix('$') {
            if name.is_empty() {
                return Err("'$' needs a sprite name".into());
            }
            let handle = *self
                .layout
                .sprites
                .get(name)
                .ok_or_else(|| format!("unknown sprite '${name}'"))?;
            self.emit(OP_PUSH);
            self.emit_u32(handle);
            return Ok(());
        }
        // '&name' is the slot index of a named slot, so a program can index an
        // array declared by the scene: '&velocity0 i +' is velocity[i].
        if let Some(name) = token.strip_prefix('&') {
            if name.is_empty() {
                return Err("'&' needs a slot name".into());
            }
            let slot = *self
                .layout
                .index
                .get(name)
                .ok_or_else(|| format!("unknown slot '&{name}'"))?;
            self.emit(OP_PUSH);
            self.emit_u32(slot as u32);
            return Ok(());
        }
        if let Some(name) = token.strip_prefix('@') {
            let slot = *self
                .layout
                .index
                .get(name)
                .ok_or_else(|| format!("unknown slot '@{name}'"))?;
            self.emit(OP_GET);
            self.emit_u16(slot);
            return Ok(());
        }
        if let Some(name) = token.strip_prefix('!') {
            let slot = *self
                .layout
                .index
                .get(name)
                .ok_or_else(|| format!("unknown slot '!{name}'"))?;
            self.emit(OP_SET);
            self.emit_u16(slot);
            return Ok(());
        }
        if let Some(opcode) = primitive(token) {
            self.emit(opcode);
            return Ok(());
        }
        if let Some(body) = self.definitions.get(token).cloned() {
            if depth >= MAX_INLINE_DEPTH {
                return Err(format!(
                    "'{token}' nests too deeply; recursion is not allowed"
                ));
            }
            let mut inner = 0;
            while inner < body.len() {
                let next = body[inner].clone();
                self.compile_token(&next, &body, &mut inner, depth + 1)?;
            }
            return Ok(());
        }

        Err(format!("unknown word '{token}'"))
    }

    fn collect_definitions(&mut self, tokens: &[String]) -> Result<(), String> {
        // Collect definitions first so program order does not matter.
        let mut index = 0;
        while index < tokens.len() {
            if tokens[index] != "def" {
                return Err(format!(
                    "expected 'def' at top level, found '{}'",
                    tokens[index]
                ));
            }
            let name = tokens.get(index + 1).ok_or("'def' needs a name")?.clone();
            if self.definitions.contains_key(&name) {
                return Err(format!("duplicate definition '{name}'"));
            }
            index += 2;
            let start = index;
            while index < tokens.len() && tokens[index] != "end" {
                index += 1;
            }
            if index >= tokens.len() {
                return Err(format!("definition '{name}' is missing 'end'"));
            }
            self.definitions.insert(name, tokens[start..index].to_vec());
            index += 1;
        }
        Ok(())
    }

    // An entry point the program does not define is a HALT, so the engine can
    // always call all three.
    fn compile_entry(&mut self, name: &str) -> Result<u32, String> {
        let at = self.here() as u32;
        if let Some(body) = self.definitions.get(name).cloned() {
            let mut position = 0;
            while position < body.len() {
                let token = body[position].clone();
                self.compile_token(&token, &body, &mut position, 0)?;
            }
        }
        self.emit(OP_HALT);
        Ok(at)
    }
}

fn primitive(word: &str) -> Option<u8> {
    Some(match word {
        "dup" => OP_DUP,
        "drop" => OP_DROP,
        "swap" => OP_SWAP,
        "over" => OP_OVER,
        "+" => OP_ADD,
        "-" => OP_SUB,
        "*" => OP_MUL,
        "/" => OP_DIV,
        "neg" => OP_NEG,
        "min" => OP_MIN,
        "max" => OP_MAX,
        "mod" => OP_MOD,
        "fx*" => OP_FX_MUL,
        "fx/" => OP_FX_DIV,
        "=" => OP_EQ,
        "<>" => OP_NE,
        "<" => OP_LT,
        ">" => OP_GT,
        "<=" => OP_LE,
        ">=" => OP_GE,
        "and" => OP_AND,
        "or" => OP_OR,
        "not" => OP_NOT,
        "random" => OP_RANDOM,
        "when" => OP_WHEN,
        "ifelse" => OP_IFELSE,
        _ => return None,
    })
}

fn tokenize(text: &str) -> Vec<String> {
    let mut tokens = Vec::new();
    for line in text.lines() {
        let line = line.split('#').next().unwrap_or("");
        for piece in line.split_whitespace() {
            // Allow "[...]" to be written without spaces around the brackets.
            let mut rest = piece;
            while !rest.is_empty() {
                if let Some(tail) = rest.strip_prefix('[') {
                    tokens.push("[".to_string());
                    rest = tail;
                } else if let Some(tail) = rest.strip_prefix(']') {
                    tokens.push("]".to_string());
                    rest = tail;
                } else {
                    let end = rest.find(['[', ']']).unwrap_or(rest.len());
                    tokens.push(rest[..end].to_string());
                    rest = &rest[end..];
                }
            }
        }
    }
    tokens
}

fn main() {
    let arguments: Vec<String> = env::args().collect();
    if arguments.len() != 4 {
        fail("usage: behaviourc <scene> <program> <output.bc>".into());
    }

    let scene_text = fs::read_to_string(&arguments[1])
        .unwrap_or_else(|error| fail(format!("{}: {error}", arguments[1])));
    let layout = parse_scene(&scene_text).unwrap_or_else(|error| fail(error));

    let program_text = fs::read_to_string(&arguments[2])
        .unwrap_or_else(|error| fail(format!("{}: {error}", arguments[2])));
    let tokens = tokenize(&program_text);
    if tokens.is_empty() {
        fail("program is empty".into());
    }

    let mut compiler = Compiler::new(&layout);
    let entries = compiler
        .collect_definitions(&tokens)
        .and_then(|()| compiler.compile_entry("init"))
        .and_then(|init| compiler.compile_entry("tick").map(|tick| (init, tick)))
        .and_then(|(init, tick)| {
            compiler
                .compile_entry("on-key")
                .map(|key| (init, tick, key))
        })
        .and_then(|(init, tick, key)| {
            compiler
                .compile_entry("on-animation-end")
                .map(|end| (init, tick, key, end))
        });
    let (init, tick, key, animation_end) = match entries {
        Ok(entries) => entries,
        Err(error) => fail(error),
    };

    let slot_count = layout.slot_count() as u16;
    let code_size = compiler.code.len() as u32;

    let mut output = Vec::new();
    output.extend_from_slice(MAGIC);
    output.extend_from_slice(&slot_count.to_le_bytes());
    output.extend_from_slice(&0u16.to_le_bytes());
    output.extend_from_slice(&init.to_le_bytes());
    output.extend_from_slice(&tick.to_le_bytes());
    output.extend_from_slice(&key.to_le_bytes());
    output.extend_from_slice(&animation_end.to_le_bytes());
    output.extend_from_slice(&code_size.to_le_bytes());
    output.extend_from_slice(&compiler.code);

    fs::write(&arguments[3], &output)
        .unwrap_or_else(|error| fail(format!("{}: {error}", arguments[3])));

    println!(
        "behaviourc: {} slots, {} bytes of code -> {}",
        slot_count, code_size, arguments[3]
    );
}

///
/// @brief Errors returned from functions.
/// Standard codes automatically generated from DJGPP's errno.h
///
typedef enum error_e {
  ERR_SUCCESS = 0,
  /* Basic ISO C Codes */
  ERR_DOM = 1,   /* Mathematics argument out of domain of function */
  ERR_RANGE = 2, /* Result too large */

  /* POSIX / General Codes */
  ERR_2BIG = 3,         /* Argument list too long */
  ERR_ACCES = 4,        /* Permission denied */
  ERR_AGAIN = 5,        /* Resource temporarily unavailable */
  ERR_BADF = 6,         /* Bad file descriptor */
  ERR_BUSY = 7,         /* Device or resource busy */
  ERR_CHILD = 8,        /* No child processes */
  ERR_DEADLK = 9,       /* Resource deadlock avoided */
  ERR_EXIST = 10,       /* File exists */
  ERR_FAULT = 11,       /* Bad address */
  ERR_FBIG = 12,        /* File too large */
  ERR_INTR = 13,        /* Interrupted system call */
  ERR_INVAL = 14,       /* Invalid argument */
  ERR_IO = 15,          /* Input/output error */
  ERR_ISDIR = 16,       /* Is a directory */
  ERR_MFILE = 17,       /* Too many open files */
  ERR_MLINK = 18,       /* Too many links */
  ERR_NAMETOOLONG = 19, /* File name too long */
  ERR_NFILE = 20,       /* Too many open files in system */
  ERR_NODEV = 21,       /* No such device */
  ERR_NOENT = 22,       /* No such file or directory */
  ERR_NOEXEC = 23,      /* Exec format error */
  ERR_NOLCK = 24,       /* No locks available */
  ERR_NOMEM = 25,       /* Cannot allocate memory */
  ERR_NOSPC = 26,       /* No space left on device */
  ERR_NOSYS = 27,       /* Function not implemented */
  ERR_NOTDIR = 28,      /* Not a directory */
  ERR_NOTEMPTY = 29,    /* Directory not empty */
  ERR_NOTTY = 30,       /* Inappropriate ioctl for device */
  ERR_NXIO = 31,        /* Device not configured */
  ERR_PERM = 32,        /* Operation not permitted */
  ERR_PIPE = 33,        /* Broken pipe */
  ERR_ROFS = 34,        /* Read-only file system */
  ERR_SPIPE = 35,       /* Illegal seek */
  ERR_SRCH = 36,        /* No such process */
  ERR_XDEV = 37,        /* Improper link */

  /* Non-POSIX / Extensions */
  ERR_NMFILE = 38,   /* No more files */
  ERR_LOOP = 39,     /* Too many levels of symbolic links */
  ERR_OVERFLOW = 40, /* Value too large to be stored in data type */
  ERR_ILSEQ = 41,    /* Illegal byte sequence */

  ERR_PNG_INVALID_SIGNATURE = 1001,
  ERR_PNG_OUT_OF_MEMORY = 1002,
  ERR_PNG_LOADING_FAILED = 1003,
} error;

const char *error_to_string(error code) {
  switch (code) {
  case ERR_DOM:
    return "Mathematics argument out of domain";
  case ERR_RANGE:
    return "Result too large";
  case ERR_2BIG:
    return "Argument list too long";
  case ERR_ACCES:
    return "Permission denied";
  case ERR_AGAIN:
    return "Resource temporarily unavailable";
  case ERR_BADF:
    return "Bad file descriptor";
  case ERR_BUSY:
    return "Device or resource busy";
  case ERR_CHILD:
    return "No child processes";
  case ERR_DEADLK:
    return "Resource deadlock avoided";
  case ERR_EXIST:
    return "File exists";
  case ERR_FAULT:
    return "Bad address";
  case ERR_FBIG:
    return "File too large";
  case ERR_INTR:
    return "Interrupted system call";
  case ERR_INVAL:
    return "Invalid argument";
  case ERR_IO:
    return "Input/output error";
  case ERR_ISDIR:
    return "Is a directory";
  case ERR_MFILE:
    return "Too many open files";
  case ERR_MLINK:
    return "Too many links";
  case ERR_NAMETOOLONG:
    return "File name too long";
  case ERR_NFILE:
    return "Too many open files in system";
  case ERR_NODEV:
    return "No such device";
  case ERR_NOENT:
    return "No such file or directory";
  case ERR_NOEXEC:
    return "Exec format error";
  case ERR_NOLCK:
    return "No locks available";
  case ERR_NOMEM:
    return "Cannot allocate memory";
  case ERR_NOSPC:
    return "No space left on device";
  case ERR_NOSYS:
    return "Function not implemented";
  case ERR_NOTDIR:
    return "Not a directory";
  case ERR_NOTEMPTY:
    return "Directory not empty";
  case ERR_NOTTY:
    return "Inappropriate ioctl for device";
  case ERR_NXIO:
    return "Device not configured";
  case ERR_PERM:
    return "Operation not permitted";
  case ERR_PIPE:
    return "Broken pipe";
  case ERR_ROFS:
    return "Read-only file system";
  case ERR_SPIPE:
    return "Illegal seek";
  case ERR_SRCH:
    return "No such process";
  case ERR_XDEV:
    return "Improper link";
  case ERR_NMFILE:
    return "No more files";
  case ERR_LOOP:
    return "Too many levels of symbolic links";
  case ERR_OVERFLOW:
    return "Value too large for data type";
  case ERR_ILSEQ:
    return "Illegal byte sequence";
  case ERR_PNG_INVALID_SIGNATURE:
    return "Invalid PNG signature (libpng)";
  case ERR_PNG_OUT_OF_MEMORY:
    return "Out of memory (libpng)";
  case ERR_PNG_LOADING_FAILED:
    return "Failed to load PNG (libpng)";
  default:
    return "Unknown error";
  }
}

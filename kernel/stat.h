#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device

struct stat {
  int dev;     // File system's disk device
  uint ino;    // Inode number
  short type;  // Type of file    文件当前位置（目录、文件、设备）
  short nlink; // Number of links to file
  uint64 size; // Size of file in bytes
};

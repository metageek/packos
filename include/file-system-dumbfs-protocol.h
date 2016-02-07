#ifndef _FILE_SYSTEM_DUMBFS_PROTOCOL_H_
#define _FILE_SYSTEM_DUMBFS_PROTOCOL_H_

#define DUMBFS_MAX_PATHLEN 511
#define DUMBFS_MAX_BUFFER 1024
#define DUMBFS_DEFAULT_PORT 5002

typedef enum {
  dumbFSRequestCmdInvalid=0,
  dumbFSRequestCmdOpen=1,
  dumbFSRequestCmdClose=2,
  dumbFSRequestCmdRead=3,
  dumbFSRequestCmdWrite=4,
  dumbFSRequestCmdDelete=5,
  dumbFSRequestCmdRename=6
} DumbFSRequestCmd;

typedef struct {
  uint16_t cmd;
  uint16_t requestId;
  union {
    struct {
      char path[DUMBFS_MAX_PATHLEN+1];
      uint32_t flags;
    } open;
    struct {
      uint32_t fileId;
    } close;
    struct {
      uint32_t fileId;
      uint32_t pos;
      uint32_t count;
    } read;
    struct {
      uint32_t fileId;
      uint32_t pos;
      uint32_t count;
      char buff[DUMBFS_MAX_BUFFER];
    } write;
    struct {
      char path[DUMBFS_MAX_PATHLEN+1];
    } delete;
    struct {
      char pathFrom[DUMBFS_MAX_PATHLEN+1];
      char pathTo[DUMBFS_MAX_PATHLEN+1];
    } rename;
  } args;
} DumbFSRequest;

typedef struct {
  uint16_t cmd,requestId,errorAsInt,reserved;
  union {
    struct {
      uint32_t fileId;
    } open;
    struct {
      uint32_t actual;
      char buff[DUMBFS_MAX_BUFFER];
    } read;
    struct {
      uint32_t actual;
    } write;
  } args;
} DumbFSReply;
#endif /*_FILE_SYSTEM_DUMBFS_PROTOCOL_H_*/

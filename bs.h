#ifndef __BS_BS_H__
#define __BS_BS_H__

enum BsStatus {
  BS_Ok,
  BS_Error,
  BS_Incomplete,
};

enum BsStatus bs_interpret(const char *source);

#endif  // __BS_BS_H__

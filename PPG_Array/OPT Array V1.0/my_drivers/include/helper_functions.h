#define BYTE0(x) (0xFF & x)
#define BYTE1(x) ((0xFF00 & x) >> 8)
#define BYTE2(x) ((0xFF0000 & x) >> 16)
#define BYTE3(x) ((0xFF000000 & x) >> 24)


#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })


#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

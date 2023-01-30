size_t strlcpy(char *dst, const char *src, size_t dsize);
#define sstrcpy(X, Y) strlcpy(X, Y, sizeof(X))

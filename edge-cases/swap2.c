typedef unsigned int uint32_t;
typedef unsigned short uint16_t;

uint32_t
swaphalves(uint32_t a)
{
    uint16_t as16bit[2],tmp;

    memcpy(as16bit, &a, sizeof(a));
    tmp = as16bit[0];
    as16bit[0] = as16bit[1];
    as16bit[1] = tmp;
    memcpy(&a, as16bit, sizeof(a));
    return a;
}

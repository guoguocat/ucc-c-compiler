typedef unsigned int uint32_t;
typedef unsigned short uint16_t;

uint32_t
swaphalves(uint32_t a)
{
    typedef union {
        uint32_t as32bit;
        uint16_t as16bit[2];
    } swapem;

    swapem s={a};
    uint16_t tmp;
    tmp=s.as16bit[0];
    s.as16bit[0]=s.as16bit[1];
    s.as16bit[1]=tmp;
    return s.as32bit;
}

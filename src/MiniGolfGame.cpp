// MiniGolfGame.cpp
// Port of tiberiusbrown/arduboy_minigolf to ESP32 + SSD1306 128×64 + MPU-6050
// Controls: tilt left/right = aim yaw | tilt fwd/back = power | button = shoot/advance

#include "MiniGolfGame.h"
#include <Arduino.h>
#include <string.h>

// ============================================================
//  Core types (mirrors game.hpp, prefixed mg_ to avoid clashes)
// ============================================================

static constexpr int     FBW           = 128;
static constexpr int     FBH           = 64;
static constexpr int     FBR           = FBH / 8;
static constexpr int     FB_FRAC_BITS  = 3;
static constexpr uint8_t FB_FRAC_COEF  = 1 << FB_FRAC_BITS;   // 8
static constexpr uint8_t FB_FRAC_MASK  = FB_FRAC_COEF - 1;

static constexpr int16_t  BALL_RADIUS    = 128;   // 256 * 0.5
static constexpr uint32_t BALL_RADIUS_SQ = uint32_t(BALL_RADIUS)*BALL_RADIUS;
static constexpr uint8_t  BOX_SIZE_FACTOR = 16;
static constexpr uint8_t  BOX_POS_FACTOR  = 64;
static constexpr uint8_t  MAX_BOXES  = 32;
static constexpr uint8_t  MAX_VERTS  = 100;
static constexpr uint8_t  MAX_FACES  = 100;
static constexpr uint8_t  NUM_LEVELS = 18;

struct mg_dvec2 { int16_t x, y; };
struct mg_vec3  { int8_t  x, y, z; };
struct mg_uvec3 { uint8_t x, y, z; };
struct mg_dvec3 { int16_t x, y, z; };

struct mg_mat3  {
    int8_t v[9];
    int8_t  operator[](int i) const { return v[i]; }
    int8_t& operator[](int i)       { return v[i]; }
};
struct mg_dmat3 {
    int16_t v[9];
    int16_t  operator[](int i) const { return v[i]; }
    int16_t& operator[](int i)       { return v[i]; }
};

struct mg_phys_box {
    mg_uvec3 size;
    mg_vec3  pos;
    uint8_t  yaw;
    int8_t   pitch;
};

struct mg_level_info_ext {
    uint8_t  num_verts;
    uint8_t  pat_faces[5];
    uint8_t  num_faces;
    uint8_t  num_boxes;
    mg_dvec3 ball_pos;
    mg_dvec3 flag_pos;
    uint8_t  par;
};

struct mg_level_info {
    int8_t           const* verts;
    uint8_t          const* faces;
    mg_phys_box      const* boxes;
    mg_level_info_ext        ext;
};

struct mg_face { uint8_t i0, i1, i2, pt; };

// Aliases so MiniGolfLevels.h (which uses the original type names) compiles here
using phys_box       = mg_phys_box;
using level_info_ext = mg_level_info_ext;
using level_info     = mg_level_info;

// Level PROGMEM data — must appear AFTER type definitions
#include "MiniGolfLevels.h"

// ============================================================
//  Utility templates
// ============================================================

template<class T> static inline T mg_min(T a, T b) { return a < b ? a : b; }
template<class T> static inline T mg_max(T a, T b) { return a > b ? a : b; }
template<class T> static inline T mg_min(T a, T b, T c) { return mg_min(mg_min(a,b),c); }
template<class T> static inline T mg_max(T a, T b, T c) { return mg_max(mg_max(a,b),c); }
template<class T> static inline T mg_clamp(T x, T lo, T hi) { return mg_min(mg_max(x,lo),hi); }
template<class T> static inline T mg_abs(T x) { return x < T(0) ? -x : x; }
template<class T> static inline void mg_swap(T& a, T& b) { T c=a; a=b; b=c; }

// ============================================================
//  Fixed-point arithmetic  (non-AVR implementations)
// ============================================================

static inline int8_t   fmuls8   (int8_t  a, int8_t   b) { return int8_t (int16_t(a)*b >> 7); }
static inline int16_t  mul_f7_s16(int16_t a, int8_t   b) { return int16_t(int32_t(a)*b >> 7); }
static inline int16_t  mul_f8_s16(int16_t a, uint8_t  b) { return int16_t(int32_t(a)*b >> 8); }
static inline int16_t  mul_f8_s16(int16_t a, uint16_t b) { return int16_t(int32_t(a)*b >> 8); }
static inline int16_t  mul_f8_s16(int16_t a, int16_t  b) { return int16_t(int32_t(a)*b >> 8); }
static inline uint16_t mul_f8_u16(uint16_t a, uint8_t  b) { return uint16_t(uint32_t(a)*b >> 8); }
static inline uint16_t mul_f8_u16(uint16_t a, uint16_t b) { return uint16_t(uint32_t(a)*b >> 8); }
static inline int16_t  mul_f15_s16(int16_t a, int16_t  b) { return int16_t(int32_t(a)*b >> 15); }
static inline int16_t  div_frac_s (int16_t x)              { return x >> FB_FRAC_BITS; }

// ============================================================
//  Lookup-table reciprocal (div.cpp)
// ============================================================

static const uint16_t DIVISORS[256] PROGMEM = {
    65535,65535,32767,21845,16383,13107,10922, 9362,
     8191, 7281, 6553, 5957, 5461, 5041, 4681, 4369,
     4095, 3855, 3640, 3449, 3276, 3120, 2978, 2849,
     2730, 2621, 2520, 2427, 2340, 2259, 2184, 2114,
     2047, 1985, 1927, 1872, 1820, 1771, 1724, 1680,
     1638, 1598, 1560, 1524, 1489, 1456, 1424, 1394,
     1365, 1337, 1310, 1285, 1260, 1236, 1213, 1191,
     1170, 1149, 1129, 1110, 1092, 1074, 1057, 1040,
     1023, 1008,  992,  978,  963,  949,  936,  923,
      910,  897,  885,  873,  862,  851,  840,  829,
      819,  809,  799,  789,  780,  771,  762,  753,
      744,  736,  728,  720,  712,  704,  697,  689,
      682,  675,  668,  661,  655,  648,  642,  636,
      630,  624,  618,  612,  606,  601,  595,  590,
      585,  579,  574,  569,  564,  560,  555,  550,
      546,  541,  537,  532,  528,  524,  520,  516,
      511,  508,  504,  500,  496,  492,  489,  485,
      481,  478,  474,  471,  468,  464,  461,  458,
      455,  451,  448,  445,  442,  439,  436,  434,
      431,  428,  425,  422,  420,  417,  414,  412,
      409,  407,  404,  402,  399,  397,  394,  392,
      390,  387,  385,  383,  381,  378,  376,  374,
      372,  370,  368,  366,  364,  362,  360,  358,
      356,  354,  352,  350,  348,  346,  344,  343,
      341,  339,  337,  336,  334,  332,  330,  329,
      327,  326,  324,  322,  321,  319,  318,  316,
      315,  313,  312,  310,  309,  307,  306,  304,
      303,  302,  300,  299,  297,  296,  295,  293,
      292,  291,  289,  288,  287,  286,  284,  283,
      282,  281,  280,  278,  277,  276,  275,  274,
      273,  271,  270,  269,  268,  267,  266,  265,
      264,  263,  262,  261,  260,  259,  258,  257,
};

static uint16_t inv8(uint8_t x) { return pgm_read_word(&DIVISORS[x]); }

static uint16_t inv16(uint16_t x) {
    if (x < 256) return 65535u;
    const uint16_t* p = &DIVISORS[x >> 8];
    uint16_t y = pgm_read_word(p);
    {
        uint16_t ty = pgm_read_word(p+1);
        uint8_t  t1 = uint8_t(x), t0 = 255-t1;
        y = mul_f8_u16(y,t0) + mul_f8_u16(ty,t1) + (y>>8);
    }
    {   // one Newton-Raphson step
        uint32_t xy = (uint32_t(y)*x)>>8;
        uint32_t t  = uint32_t(0x20000u) - xy;
        y = uint16_t((uint32_t(t)*y)>>16);
    }
    return y;
}

// ============================================================
//  sincos.cpp
// ============================================================

static const int8_t SINE_QUAD[65] PROGMEM = {
    0,3,6,9,12,15,18,21,24,27,30,34,37,39,42,45,
    48,51,54,57,60,62,65,68,70,73,75,78,80,83,85,87,
    90,92,94,96,98,100,102,104,106,107,109,110,112,113,
    115,116,117,118,120,121,122,122,123,124,125,125,126,
    126,126,127,127,127,127
};
static int8_t fsin8(uint8_t a) {
    uint8_t i = a & 0x3f;
    if (a & 0x40) i = 64-i;
    int8_t r = (int8_t)pgm_read_byte(&SINE_QUAD[i]);
    if (a & 0x80) r = -r;
    return r;
}
static int8_t fcos8(uint8_t a) { return fsin8(a + 0x40); }

static const int16_t SINE_QUAD_16[65] PROGMEM = {
    0,804,1608,2410,3212,4011,4808,5602,6393,7179,7962,
    8739,9512,10278,11039,11793,12539,13279,14010,14732,
    15446,16151,16846,17530,18204,18868,19519,20159,20787,
    21403,22005,22594,23170,23731,24279,24811,25329,25832,
    26319,26790,27245,27683,28105,28510,28898,29268,29621,
    29956,30273,30571,30852,31113,31356,31580,31785,31971,
    32137,32285,32412,32521,32609,32678,32728,32757,32766
};
static int16_t fsin16_h(uint8_t a) {
    uint8_t i = a & 0x3f;
    if (a & 0x40) i = 64-i;
    int16_t r = (int16_t)pgm_read_word(&SINE_QUAD_16[i]);
    if (a & 0x80) r = -r;
    return r;
}
static int16_t fsin16(uint16_t a) {
    uint8_t hi = uint8_t(a>>8);
    int16_t s0=fsin16_h(hi), s1=fsin16_h(hi+1);
    uint8_t f1=uint8_t(a); uint16_t f0=256-f1;
    return mul_f8_s16(s0,f0)+mul_f8_s16(s1,f1);
}
static int16_t fcos16(uint16_t a) { return fsin16(a+0x4000); }

static const uint16_t ATAN_TABLE[33] PROGMEM = {
    0,326,651,975,1297,1617,1933,2246,2555,2860,3159,3453,3742,
    4025,4302,4572,4836,5094,5344,5589,5826,6058,6282,6500,6712,
    6917,7117,7310,7498,7679,7856,8026,8192
};
static int16_t mg_atan2(int16_t y, int16_t x) {
    uint8_t f=0;
    if (y<0){f|=1;y=-y;} if (x<0){f|=2;x=-x;} if (x<y){f|=4;mg_swap(x,y);}
    uint16_t ratio;
    if (x<256) ratio=uint16_t((uint32_t(inv8(uint8_t(x)))*uint16_t(y))>>3);
    else       ratio=uint16_t((uint32_t(inv16(uint16_t(x)))*uint16_t(y))>>11);
    uint8_t  idx=uint8_t(ratio>>8), f1=uint8_t(ratio);
    uint16_t f0=256-f1;
    uint16_t t=mul_f8_u16(pgm_read_word(&ATAN_TABLE[idx]),f0)
              +mul_f8_u16(pgm_read_word(&ATAN_TABLE[idx+1]),f1);
    if (f&4) t=16384-t; if (f&2) t=32768-t; if (f&1) t=(uint16_t)(-t);
    return (int16_t)t;
}

// ============================================================
//  mat.cpp — rotation + matrix-vector multiply
// ============================================================

static void rotation(mg_mat3& m, uint8_t yaw, int8_t pitch) {
    int8_t sA=fsin8((uint8_t)pitch),cA=fcos8((uint8_t)pitch);
    int8_t sB=fsin8(yaw),cB=fcos8(yaw);
    m[0]=cB;            m[1]=0;   m[2]=sB;
    m[3]=fmuls8(sA,sB); m[4]=cA;  m[5]=-fmuls8(sA,cB);
    m[6]=-fmuls8(cA,sB);m[7]=sA;  m[8]=fmuls8(cA,cB);
}
static void rotation16(mg_dmat3& m, uint16_t yaw, int16_t pitch) {
    int16_t sA=fsin16((uint16_t)pitch),cA=fcos16((uint16_t)pitch);
    int16_t sB=fsin16(yaw),cB=fcos16(yaw);
    m[0]=cB;m[1]=0;m[2]=sB;
    m[3]=mul_f15_s16(sA,sB); m[4]=cA; m[5]=-mul_f15_s16(sA,cB);
    m[6]=-mul_f15_s16(cA,sB);m[7]=sA; m[8]=mul_f15_s16(cA,cB);
}
static void rotation_phys(mg_mat3& m, uint8_t yaw, int8_t pitch) {
    int8_t sA=fsin8((uint8_t)pitch),cA=fcos8((uint8_t)pitch);
    int8_t sB=fsin8(yaw),cB=fcos8(yaw);
    m[0]=cB;             m[1]=fmuls8(sA,sB); m[2]=fmuls8(cA,sB);
    m[3]=0;              m[4]=cA;             m[5]=-sA;
    m[6]=-sB;            m[7]=fmuls8(sA,cB); m[8]=fmuls8(cA,cB);
}

static mg_dvec3 matvec(const mg_mat3& m, mg_vec3 v) {
    return {int16_t(v.x*m[0]+v.y*m[1]+v.z*m[2]),
            int16_t(v.x*m[3]+v.y*m[4]+v.z*m[5]),
            int16_t(v.x*m[6]+v.y*m[7]+v.z*m[8])};
}
static mg_dvec3 matvec_t(const mg_mat3& m, mg_dvec3 v) {
    return {int16_t(mul_f7_s16(v.x,m[0])+mul_f7_s16(v.y,m[3])+mul_f7_s16(v.z,m[6])),
            int16_t(mul_f7_s16(v.x,m[1])+mul_f7_s16(v.y,m[4])+mul_f7_s16(v.z,m[7])),
            int16_t(mul_f7_s16(v.x,m[2])+mul_f7_s16(v.y,m[5])+mul_f7_s16(v.z,m[8]))};
}
static mg_dvec3 matvec(const mg_mat3& m, mg_dvec3 v) {
    return {int16_t(mul_f7_s16(v.x,m[0])+mul_f7_s16(v.y,m[1])+mul_f7_s16(v.z,m[2])),
            int16_t(mul_f7_s16(v.x,m[3])+mul_f7_s16(v.y,m[4])+mul_f7_s16(v.z,m[5])),
            int16_t(mul_f7_s16(v.x,m[6])+mul_f7_s16(v.y,m[7])+mul_f7_s16(v.z,m[8]))};
}
static mg_dvec3 matvec(const mg_dmat3& m, mg_dvec3 v) {
    return {int16_t(mul_f15_s16(v.x,m[0])+mul_f15_s16(v.y,m[1])+mul_f15_s16(v.z,m[2])),
            int16_t(mul_f15_s16(v.x,m[3])+mul_f15_s16(v.y,m[4])+mul_f15_s16(v.z,m[5])),
            int16_t(mul_f15_s16(v.x,m[6])+mul_f15_s16(v.y,m[7])+mul_f15_s16(v.z,m[8]))};
}

static uint16_t inv_sqrt(uint16_t a) {
    uint16_t x=0x100;
    for (uint8_t i=0;i<8;++i){
        uint16_t t=mul_f8_u16(x,x); t=mul_f8_u16(a,t); t>>=1;
        x=mul_f8_u16(uint16_t(0x180-t),x);
    }
    return x;
}
static mg_dvec3 normalized(mg_dvec3 v) {
    while (mg_max(mg_abs(v.x),mg_abs(v.y),mg_abs(v.z))>=256){v.x/=2;v.y/=2;v.z/=2;}
    uint16_t d=uint16_t(mul_f8_s16(v.x,v.x))+uint16_t(mul_f8_s16(v.y,v.y))+uint16_t(mul_f8_s16(v.z,v.z));
    d=inv_sqrt(d);
    v.x=mul_f8_s16(v.x,d); v.y=mul_f8_s16(v.y,d); v.z=mul_f8_s16(v.z,d);
    return v;
}
static int16_t mg_dot(mg_dvec3 a, mg_dvec3 b) {
    return int16_t(mul_f8_s16(a.x,b.x)+mul_f8_s16(a.y,b.y)+mul_f8_s16(a.z,b.z));
}

// ============================================================
//  Module-level state  (mirrors original globals)
// ============================================================

static uint8_t* s_buf;  // set to d.getBuffer() each frame

// physics
static mg_dvec3  s_ball, s_ball_vel, s_ball_vel_ang;

// camera
static mg_dvec3  s_cam;
static uint16_t  s_yaw;
static int16_t   s_pitch;

// level scratch buffers (populated by load_level_from_prog each frame)
static int8_t       s_vy       [MAX_VERTS];
static int8_t       s_vxz      [MAX_VERTS * 2];
static uint16_t     s_vdist    [MAX_VERTS];
static uint8_t      s_faces_buf[MAX_FACES * 3];
static uint16_t     s_fdist    [MAX_FACES];
static mg_phys_box  s_boxes    [MAX_BOXES];
static int16_t      s_tvz      [MAX_VERTS];

// render non-buffer
static mg_face   s_fs    [MAX_FACES];
static uint8_t   s_forder[MAX_FACES];
static mg_dvec2  s_vs    [MAX_VERTS];

// game state
static mg_level_info_ext s_levelext;

enum class mg_st : uint8_t { LEVEL, AIM, ROLLING, HOLE, SCORE };
static mg_st    s_state;
static uint8_t  s_nframe;
static uint8_t  s_leveli;
static uint8_t  s_shots[NUM_LEVELS];
static uint16_t s_yaw_aim;
static uint8_t  s_power_aim;
static int16_t  s_pitch_aim;
static mg_dvec3 s_prev_ball;
static uint16_t s_yaw_level;
static bool     s_game_done;

struct CamCtrl { int16_t dydt; };
static CamCtrl s_ctrl[5];

// ============================================================
//  draw.cpp — pixel/line/triangle/circle drawing into s_buf
// ============================================================

static const uint16_t PATTERNS[5] PROGMEM = {
    0x0000u, 0xaa00u, 0xaa55u, 0xff55u, 0xffffu
};
static const uint8_t YMASK0[8] PROGMEM = {0xff,0xfe,0xfc,0xf8,0xf0,0xe0,0xc0,0x80};
static const uint8_t YMASK1[8] PROGMEM = {0x01,0x03,0x07,0x0f,0x1f,0x3f,0x7f,0xff};

static void draw_vline(uint8_t x, int16_t y0, int16_t y1, uint16_t pat) {
    if (y0>y1 || y1<0 || y0>=FBH || x>=FBW) return;
    uint8_t ty0=(uint8_t)mg_max<int16_t>(y0,0);
    uint8_t ty1=(uint8_t)mg_min<int16_t>(y1,FBH-1);
    uint8_t t0=ty0&0xf8, t1=ty1&0xf8;
    uint8_t m0=pgm_read_byte(&YMASK0[ty0&7]);
    uint8_t m1=pgm_read_byte(&YMASK1[ty1&7]);
    uint8_t pattern=(x&1)?uint8_t(pat):uint8_t(pat>>8);
    uint8_t* p=s_buf+uint16_t(t0)*(FBW/8)+x;
    if (t0==t1){uint8_t m=m0&m1; *p=(*p|(pattern&m))&(pattern|~m); return;}
    *p=(*p|(pattern&m0))&(pattern|~m0); p+=FBW;
    for (int8_t t=t1-t0-8; t>0; t-=8){*p=pattern; p+=FBW;}
    *p=(*p|(pattern&m1))&(pattern|~m1);
}

static int16_t mg_interp(int16_t a, int16_t b, int16_t c, int16_t x, int16_t z) {
    if (a==c) return x;
    uint16_t xz=(x<z)?uint16_t(z-x):uint16_t(x-z);
    uint32_t p32=uint32_t(xz)*uint16_t(b-a);
    uint16_t ac=uint16_t(c-a);
    int16_t  t;
    if (ac>=256) t=int16_t(((p32>>8)*inv16(ac))>>16);
    else         t=int16_t((p32*inv8(uint8_t(ac)))>>16);
    if (x>z) t=-t;
    return x+t;
}

static void draw_tri_seg(int16_t ax,int16_t ay0,int16_t ay1,
                         int16_t bx,int16_t by0,int16_t by1, uint16_t pat) {
    if (ax>=bx) return;
    if (ay0>ay1||by0>by1){mg_swap(ay0,ay1);mg_swap(by0,by1);}
    int16_t dx=bx-ax;
    int16_t dy0=mg_abs<int16_t>(by0-ay0), dy1=mg_abs<int16_t>(by1-ay1);
    int16_t e0,e1;
    {
        uint8_t m0=uint8_t(ay0&FB_FRAC_MASK), m1=uint8_t(ay1&FB_FRAC_MASK);
        if (ay0>by0) m0=(FB_FRAC_COEF-m0)&FB_FRAC_MASK;
        if (ay1>by1) m1=(FB_FRAC_COEF-m1)&FB_FRAC_MASK;
        int8_t tax=int8_t((ax&FB_FRAC_MASK)-(FB_FRAC_COEF/2));
        e0=div_frac_s(int16_t(dx*(m0-FB_FRAC_COEF)-dy0*tax));
        e1=div_frac_s(int16_t(dx*(m1-FB_FRAC_COEF)-dy1*tax));
    }
    int8_t sy0=(ay0<by0)?1:-1, sy1=(ay1<by1)?1:-1;
    if (ay0>by0) ay0-=1;
    if (ay1>by1) ay1-=1;
    int16_t pxa=div_frac_s(ax);
    int16_t pxb=div_frac_s(bx+(FB_FRAC_COEF/2-1));
    int16_t py0=div_frac_s(ay0), py1=div_frac_s(ay1);
    while (pxa!=pxb) {
        while (e0>0){py0+=sy0;e0-=dx;}
        while (e1>0){py1+=sy1;e1-=dx;}
        draw_vline(uint8_t(pxa),py0,py1,pat);
        pxa++; e0+=dy0; e1+=dy1;
    }
}

static void draw_tri(mg_dvec2 v0,mg_dvec2 v1,mg_dvec2 v2,uint8_t pati) {
    if (mg_max(v0.y,v1.y,v2.y)<0) return;
    if (mg_min(v0.y,v1.y,v2.y)>FBH*FB_FRAC_COEF) return;
    if (v0.x>v1.x) mg_swap(v0,v1);
    if (v1.x>v2.x) mg_swap(v1,v2);
    if (v0.x>v1.x) mg_swap(v0,v1);
    if (v2.x<0||v0.x>=FBW*FB_FRAC_COEF||v0.x==v2.x) return;
    int16_t ty=mg_interp(v0.x,v1.x,v2.x,v0.y,v2.y);
    uint16_t pat=pgm_read_word(&PATTERNS[pati]);
    {
        int16_t ax=v0.x,bx=v1.x,ay0=v0.y,ay1=v0.y,by0=v1.y,by1=ty;
        if (ax<0){ay0=mg_interp(ax,0,bx,ay0,by0);ay1=mg_interp(ax,0,bx,ay1,by1);ax=0;}
        if (bx>FBW*FB_FRAC_COEF){by0=mg_interp(ax,FBW*FB_FRAC_COEF,bx,ay0,by0);by1=mg_interp(ax,FBW*FB_FRAC_COEF,bx,ay1,by1);bx=FBW*FB_FRAC_COEF;}
        draw_tri_seg(ax,ay0,ay1,bx,by0,by1,pat);
    }
    {
        int16_t ax=v1.x,bx=v2.x,ay0=v1.y,ay1=ty,by0=v2.y,by1=v2.y;
        if (ax<0){ay0=mg_interp(ax,0,bx,ay0,by0);ay1=mg_interp(ax,0,bx,ay1,by1);ax=0;}
        if (bx>FBW*FB_FRAC_COEF){by0=mg_interp(ax,FBW*FB_FRAC_COEF,bx,ay0,by0);by1=mg_interp(ax,FBW*FB_FRAC_COEF,bx,ay1,by1);bx=FBW*FB_FRAC_COEF;}
        draw_tri_seg(ax,ay0,ay1,bx,by0,by1,pat);
    }
}

static void draw_circle_px(uint8_t x, int16_t y, bool set) {
    if (x>=FBW||y<0||y>=FBH) return;
    uint8_t* p=s_buf+(uint16_t(y&0x38)<<4)+x;
    if (set) *p|=(1<<(y&7)); else *p&=~(1<<(y&7));
}
static void ball_vline_seg(int16_t cx,int16_t cy,int16_t x,int16_t y,uint16_t pat) {
    draw_vline(uint8_t(cx+x),cy-y,cy+y,pat); draw_vline(uint8_t(cx-x),cy-y,cy+y,pat);
    draw_vline(uint8_t(cx+y),cy-x,cy+x,pat); draw_vline(uint8_t(cx-y),cy-x,cy+x,pat);
}
static void ball_outline_seg(int16_t cx,int16_t cy,int16_t x,int16_t y) {
    draw_circle_px(uint8_t(cx+x),cy-y,false); draw_circle_px(uint8_t(cx+x),cy+y,false);
    draw_circle_px(uint8_t(cx-x),cy-y,false); draw_circle_px(uint8_t(cx-x),cy+y,false);
    draw_circle_px(uint8_t(cx+y),cy-x,false); draw_circle_px(uint8_t(cx+y),cy+x,false);
    draw_circle_px(uint8_t(cx-y),cy-x,false); draw_circle_px(uint8_t(cx-y),cy+x,false);
}

static void draw_ball_filled(mg_dvec2 c, uint16_t r, uint16_t pat) {
    if (c.x<-(int16_t)r||c.y<-(int16_t)r) return;
    if (c.x>FBW*FB_FRAC_COEF+(int16_t)r||c.y>FBW*FB_FRAC_COEF+(int16_t)r) return;
    int16_t cx=div_frac_s(c.x),cy=div_frac_s(c.y); r>>=FB_FRAC_BITS;
    int16_t dx=12,dy=8-(int16_t)r*8,e=5-(int16_t)r*4,y=(int16_t)r;
    for (int16_t x=0;x<=y;++x){
        ball_vline_seg(cx,cy,x,y,pat);
        if (e>=0){e+=dy;dy+=8;y--;}
        e+=dx;dx+=8;
    }
}
static void draw_ball_outline(mg_dvec2 c, uint16_t r) {
    if (c.x<-(int16_t)r||c.y<-(int16_t)r) return;
    if (c.x>FBW*FB_FRAC_COEF+(int16_t)r||c.y>FBW*FB_FRAC_COEF+(int16_t)r) return;
    int16_t cx=div_frac_s(c.x),cy=div_frac_s(c.y); r>>=FB_FRAC_BITS;
    int16_t dx=12,dy=8-(int16_t)r*8,e=5-(int16_t)r*4,y=(int16_t)r;
    for (int16_t x=0;x<=y;++x){
        ball_outline_seg(cx,cy,x,y);
        if (e>=0){e+=dy;dy+=8;y--;}
        e+=dx;dx+=8;
    }
}

// ============================================================
//  Level loader
// ============================================================

static void load_level_from_prog() {
    const mg_level_info* lv = &LEVELS_DEFAULT[s_leveli];
    memcpy_P(&s_levelext, &lv->ext, sizeof(mg_level_info_ext));
    {
        int8_t*       pvy  = s_vy;
        int8_t*       pvxz = s_vxz;
        const int8_t* pv   = (const int8_t*)pgm_read_ptr(&lv->verts);
        for (uint8_t i=0;i<s_levelext.num_verts;++i){
            *pvxz++=pgm_read_byte(pv++);
            *pvy++ =pgm_read_byte(pv++);
            *pvxz++=pgm_read_byte(pv++);
        }
    }
    memcpy_P(s_faces_buf,(const void*)pgm_read_ptr(&lv->faces),s_levelext.num_faces*3);
    memcpy_P(s_boxes,    (const void*)pgm_read_ptr(&lv->boxes),sizeof(mg_phys_box)*s_levelext.num_boxes);
}

static uint8_t get_par(uint8_t i) {
    // Read par field from PROGMEM level_info
    const mg_level_info* lv = &LEVELS_DEFAULT[i];
    uint8_t par;
    memcpy_P(&par, &lv->ext.par, 1);
    return par;
}

// ============================================================
//  Physics
// ============================================================

static constexpr int16_t  PHYS_GRAVITY    = 48;
static constexpr int16_t  PHYS_MAX_VEL    = 256*64;
static constexpr uint8_t  PHYS_RESTITUTION = uint8_t(256*0.75);
static constexpr int16_t  PHYS_STOP_VEL   = 256;
static constexpr uint8_t  PHYS_STOP_STEPS = 30;
static constexpr uint16_t PHYS_MAX_STEPS  = 30*60;
static uint8_t  s_stop_steps;
static uint16_t s_phys_steps;

static uint32_t sqdist_aabb(mg_dvec3 sz, mg_dvec3 pt) {
    uint32_t d=0;
    auto chk=[&](int16_t pv,int16_t bv){
        if(pv<-bv){int16_t e=pv+bv;d+=uint32_t(e)*e;}
        else if(pv>bv){int16_t e=pv-bv;d+=uint32_t(e)*e;}
    };
    chk(pt.x,sz.x); chk(pt.y,sz.y); chk(pt.z,sz.z);
    return d;
}

static mg_dvec3 cross3(mg_dvec3 a, mg_dvec3 b) {
    return {int16_t(mul_f8_s16(a.y,b.z)-mul_f8_s16(a.z,b.y)),
            int16_t(mul_f8_s16(a.z,b.x)-mul_f8_s16(a.x,b.z)),
            int16_t(mul_f8_s16(a.x,b.y)-mul_f8_s16(a.y,b.x))};
}

static void physics_collision(const mg_phys_box& b) {
    mg_mat3  m;
    mg_dvec3 pt = {int16_t(s_ball.x-b.pos.x*(int16_t)BOX_POS_FACTOR),
                   int16_t(s_ball.y-b.pos.y*(int16_t)BOX_POS_FACTOR),
                   int16_t(s_ball.z-b.pos.z*(int16_t)BOX_POS_FACTOR)};
    bool rot=(b.yaw!=0||b.pitch!=0);
    if (rot){rotation_phys(m,b.yaw,b.pitch); pt=matvec_t(m,pt);}
    mg_dvec3 bsz={int16_t(b.size.x*(int16_t)BOX_SIZE_FACTOR),
                  int16_t(b.size.y*(int16_t)BOX_SIZE_FACTOR),
                  int16_t(b.size.z*(int16_t)BOX_SIZE_FACTOR)};
    if (sqdist_aabb(bsz,pt)>BALL_RADIUS_SQ) return;
    mg_dvec3 cpt={mg_clamp<int16_t>(pt.x,-bsz.x,bsz.x),
                  mg_clamp<int16_t>(pt.y,-bsz.y,bsz.y),
                  mg_clamp<int16_t>(pt.z,-bsz.z,bsz.z)};
    mg_dvec3 normal={int16_t(pt.x-cpt.x),int16_t(pt.y-cpt.y),int16_t(pt.z-cpt.z)};
    if (!(normal.x|normal.y|normal.z)) return;
    normal=normalized(normal);
    if (rot) normal=matvec(m,normal);
    int16_t nd=mg_dot(s_ball_vel,normal);
    if (nd>0) return;
    mg_dvec3 tangent=normalized({int16_t(s_ball_vel.x-mul_f8_s16(nd,normal.x)),
                                  int16_t(s_ball_vel.y-mul_f8_s16(nd,normal.y)),
                                  int16_t(s_ball_vel.z-mul_f8_s16(nd,normal.z))});
    mg_dvec3 ct=cross3(s_ball_vel_ang,normal);
    mg_dvec3 vp={int16_t(s_ball_vel.x-ct.x/2),
                 int16_t(s_ball_vel.y-ct.y/2),
                 int16_t(s_ball_vel.z-ct.z/2)};
    int16_t t0=-mg_dot(vp,normal); t0+=mul_f8_s16(t0,PHYS_RESTITUTION);
    int16_t t1=mul_f8_s16(-mg_dot(vp,tangent),uint8_t(256/3));
    mg_dvec3 Jv={int16_t(mul_f8_s16(t0,normal.x) +mul_f8_s16(t1,tangent.x)),
                 int16_t(mul_f8_s16(t0,normal.y) +mul_f8_s16(t1,tangent.y)),
                 int16_t(mul_f8_s16(t0,normal.z) +mul_f8_s16(t1,tangent.z))};
    s_ball_vel.x+=Jv.x; s_ball_vel.y+=Jv.y; s_ball_vel.z+=Jv.z;
    mg_dvec3 av=cross3(Jv,normal);
    s_ball_vel_ang.x+=av.x*4; s_ball_vel_ang.y+=av.y*4; s_ball_vel_ang.z+=av.z*4;
}

static void phys_main_step() {
    s_ball_vel.y-=PHYS_GRAVITY;
    s_ball_vel.x=mg_clamp<int16_t>(s_ball_vel.x,-PHYS_MAX_VEL,PHYS_MAX_VEL);
    s_ball_vel.y=mg_clamp<int16_t>(s_ball_vel.y,-PHYS_MAX_VEL,PHYS_MAX_VEL);
    s_ball_vel.z=mg_clamp<int16_t>(s_ball_vel.z,-PHYS_MAX_VEL,PHYS_MAX_VEL);
    s_ball.x+=int8_t(uint16_t(s_ball_vel.x+0x80)>>8);
    s_ball.y+=int8_t(uint16_t(s_ball_vel.y+0x80)>>8);
    s_ball.z+=int8_t(uint16_t(s_ball_vel.z+0x80)>>8);
    for (uint8_t i=0;i<s_levelext.num_boxes;++i)
        physics_collision(s_boxes[i]);
}

static bool physics_step() {
    for (uint8_t i=0;i<4;++i){
        phys_main_step();
        s_ball_vel_ang.x=int16_t(int32_t(s_ball_vel_ang.x)*255>>8);
        s_ball_vel_ang.y=int16_t(int32_t(s_ball_vel_ang.y)*255>>8);
        s_ball_vel_ang.z=int16_t(int32_t(s_ball_vel_ang.z)*255>>8);
    }
    ++s_phys_steps;
    if (mg_max(mg_abs(s_ball_vel.x),mg_abs(s_ball_vel.y),mg_abs(s_ball_vel.z))<PHYS_STOP_VEL)
        ++s_stop_steps;
    else s_stop_steps=0;
    bool done=(s_phys_steps>=PHYS_MAX_STEPS||s_stop_steps>=PHYS_STOP_STEPS);
    if (done){s_phys_steps=0;s_stop_steps=0;s_ball_vel={};}
    return done;
}

static bool ball_in_hole() {
    mg_dvec3 f=s_levelext.flag_pos;
    return mg_abs<int16_t>(f.x-s_ball.x)<256
        && mg_abs<int16_t>(f.y-s_ball.y)<256
        && mg_abs<int16_t>(f.z-s_ball.z)<256;
}

// ============================================================
//  Camera
// ============================================================

static int16_t ctrl_step(CamCtrl& c, int16_t y, int16_t x, uint8_t speed) {
    int16_t diff=x-y, d2=diff/4-c.dydt;
    y+=mul_f8_s16(c.dydt,speed); c.dydt+=mul_f8_s16(d2,speed);
    return y;
}
static void update_camera(mg_dvec3 tc, uint16_t ty, int16_t tp, uint8_t ms, uint8_t ls) {
    s_cam.x=ctrl_step(s_ctrl[0],s_cam.x,tc.x,ms);
    s_cam.y=ctrl_step(s_ctrl[1],s_cam.y,tc.y,ms);
    s_cam.z=ctrl_step(s_ctrl[2],s_cam.z,tc.z,ms);
    s_yaw  =(uint16_t)ctrl_step(s_ctrl[3],(int16_t)s_yaw,(int16_t)ty,ls);
    s_pitch= ctrl_step(s_ctrl[4],s_pitch,tp,ls);
}
static void update_cam_lookat_fa(mg_dvec3 tgt, uint16_t ty, int16_t tp,
                                  uint16_t dist, uint8_t ms, uint8_t ls) {
    mg_dvec3 tc=tgt; mg_mat3 m;
    rotation_phys(m,uint8_t(-(ty>>8)),int8_t(-uint16_t(tp>>8)));
    mg_dvec3 dv=matvec(m,mg_dvec3{0,0,(int16_t)dist});
    tc.x+=dv.x; tc.y+=dv.y; tc.z+=dv.z;
    update_camera(tc,ty,tp,ms,ls);
}
static void update_cam_lookat(mg_dvec3 tgt, uint16_t ty, int16_t tp,
                               uint16_t dist, uint8_t ms, uint8_t ls) {
    mg_dvec3 tc=tgt; mg_mat3 m;
    rotation_phys(m,uint8_t(-(s_yaw>>8)),int8_t(-uint16_t(s_pitch>>8)));
    mg_dvec3 dv=matvec(m,mg_dvec3{0,0,(int16_t)dist});
    tc.x+=dv.x; tc.y+=dv.y; tc.z+=dv.z;
    update_camera(tc,ty,tp,ms,ls);
}
static uint16_t yaw_to_flag() {
    mg_dvec3 f=s_levelext.flag_pos;
    return (uint16_t)mg_atan2(f.z-s_ball.z,f.x-s_ball.x)+16384;
}
static void update_cam_follow_ball(uint16_t dist, uint8_t ms, uint8_t ls) {
    update_cam_lookat(s_ball, yaw_to_flag(), 6500, dist, ms, ls);
}

// ============================================================
//  render_scene.cpp — full 3D pipeline
// ============================================================

static constexpr int16_t ZNEAR = 256;

static void reset_forder() {
    for (uint8_t i=0;i<MAX_FACES;++i) s_forder[i]=i;
}
static uint8_t add_vertex(const mg_dmat3& m, mg_dvec3 dv, uint8_t nv) {
    dv.x-=s_cam.x; dv.y-=s_cam.y; dv.z-=s_cam.z;
    s_vdist[nv]=uint16_t(uint32_t(int32_t(dv.x)*dv.x+int32_t(dv.y)*dv.y+int32_t(dv.z)*dv.z)>>16);
    dv=matvec(m,dv); dv.z=-dv.z;
    s_vs[nv]={dv.x,dv.y}; s_tvz[nv]=dv.z;
    return nv+1;
}
static uint16_t calc_fdist(uint8_t i0,uint8_t i1,uint8_t i2,
                            int16_t y0,int16_t y1,int16_t y2) {
    uint16_t d=s_vdist[i0]+s_vdist[i1]+s_vdist[i2];
    int16_t  a0=mg_abs<int16_t>(s_cam.y-y0),a1=mg_abs<int16_t>(s_cam.y-y1),a2=mg_abs<int16_t>(s_cam.y-y2);
    uint16_t am=(uint16_t)mg_min(a0,a1,a2);
    d+=uint16_t(uint32_t(am)*am>>16)*16;
    return d;
}
static mg_dvec2 interpz(mg_dvec2 a,int16_t az,mg_dvec2 b,int16_t bz) {
    return {mg_interp(az,ZNEAR,bz,a.x,b.x), mg_interp(az,ZNEAR,bz,a.y,b.y)};
}

static void render_scene() {
    mg_dmat3 m; rotation16(m,s_yaw,s_pitch);
    uint8_t nv=0,nf=0;
    bool ball_valid=false;
    constexpr uint8_t balli0=0,balli1=1;

    // ball
    if (!ball_in_hole()) {
        nv=add_vertex(m,s_ball,nv);
        if (s_tvz[0]>=ZNEAR) {
            ball_valid=true;
            s_vs[balli1]=s_vs[balli0]; s_vs[balli1].x+=128;
            s_tvz[balli1]=s_tvz[balli0]; s_vdist[balli1]=s_vdist[balli0];
            int16_t by=s_ball.y;
            s_fdist[0]=calc_fdist(balli0,balli0,balli0,by,by,by)+400;
            s_fs[0].pt=255; ++nf; ++nv;
        }
    }

    // flag
    {
        bool fv=true; uint8_t fi=nv;
        mg_dvec3 dv=s_levelext.flag_pos; dv.y+=256;
        int16_t dvy0=dv.y;
        nv=add_vertex(m,dv,nv); fv&=(s_tvz[nv-1]>=ZNEAR);
        {mg_dvec2 tv=s_vs[nv-1];tv.x+=48;s_vs[nv]=tv;s_tvz[nv]=s_tvz[nv-1];++nv;}
        dv.y+=256*4; int16_t dvy1=dv.y;
        nv=add_vertex(m,dv,nv); fv&=(s_tvz[nv-1]>=ZNEAR);
        {mg_dvec2 tv=s_vs[nv-1];tv.x+=48;s_vs[nv]=tv;s_tvz[nv]=s_tvz[nv-1];++nv;}
        dv.y+=256*2; nv=add_vertex(m,dv,nv); fv&=(s_tvz[nv-1]>=ZNEAR);
        dv.y-=256; dv.x+=fsin8(s_nframe<<3); dv.z-=256*2;
        nv=add_vertex(m,dv,nv); fv&=(s_tvz[nv-1]>=ZNEAR);
        if (fv) {
            s_fdist[nf]=calc_fdist(fi,fi+1,fi+2,dvy0,dvy0,dvy1);
            s_fs[nf++]={uint8_t(fi),uint8_t(fi+1),uint8_t(fi+2),4};
            s_fdist[nf]=calc_fdist(fi+1,fi+2,fi+3,dvy0,dvy1,dvy1);
            s_fs[nf++]={uint8_t(fi+1),uint8_t(fi+2),uint8_t(fi+3),4};
            s_fdist[nf]=calc_fdist(fi+3,fi+4,fi+5,dvy1,dvy1,dvy1);
            s_fs[nf++]={uint8_t(fi+2),uint8_t(fi+4),uint8_t(fi+5),3};
        }
    }

    uint8_t voff=nv;

    // level vertices
    for (uint8_t j=0;j<s_levelext.num_verts;++j) {
        mg_dvec3 dv={(int16_t)(s_vxz[j*2+0]*64),(int16_t)(s_vy[j]*64),(int16_t)(s_vxz[j*2+1]*64)};
        nv=add_vertex(m,dv,nv);
    }

    // assemble faces
    uint8_t begin_nf=nf;
    for (uint8_t pat=0,tnf=0,i=0;pat<5;++pat) {
        tnf+=s_levelext.pat_faces[pat];
        for (;i<tnf;++i) {
            uint8_t i0=s_faces_buf[i*3+0]+voff;
            uint8_t i1=s_faces_buf[i*3+1]+voff;
            uint8_t i2=s_faces_buf[i*3+2]+voff;
            if (s_tvz[i0]<ZNEAR&&s_tvz[i1]<ZNEAR&&s_tvz[i2]<ZNEAR) continue;
            if (nf<MAX_FACES) s_fs[nf++]={i0,i1,i2,pat};
        }
    }

    // fdist + near-plane clip
    uint8_t end_nf=nf;
    for (uint8_t i=begin_nf;i<end_nf;++i) {
        mg_face f=s_fs[i];
        int16_t z0=s_tvz[f.i0],z1=s_tvz[f.i1],z2=s_tvz[f.i2];
        uint8_t behind=0;
        if(z0<ZNEAR)behind|=1; if(z1<ZNEAR)behind|=2; if(z2<ZNEAR)behind|=4;
        {
            int16_t y0=(int16_t)s_vy[f.i0-voff]*64;
            int16_t y1=(int16_t)s_vy[f.i1-voff]*64;
            int16_t y2=(int16_t)s_vy[f.i2-voff]*64;
            s_fdist[i]=calc_fdist(f.i0,f.i1,f.i2,y0,y1,y2);
        }
        if ((behind&(behind-1))!=0) {
            if (nv+2>MAX_VERTS) continue;
            mg_dvec2 nv0,nv1; uint8_t ib;
            if      (!(behind&1)){ib=f.i0;nv0=interpz(s_vs[f.i1],z1,s_vs[f.i0],z0);nv1=interpz(s_vs[f.i2],z2,s_vs[f.i0],z0);}
            else if (!(behind&2)){ib=f.i1;nv0=interpz(s_vs[f.i0],z0,s_vs[f.i1],z1);nv1=interpz(s_vs[f.i2],z2,s_vs[f.i1],z1);}
            else                 {ib=f.i2;nv0=interpz(s_vs[f.i0],z0,s_vs[f.i2],z2);nv1=interpz(s_vs[f.i1],z1,s_vs[f.i2],z2);}
            s_vs[nv]=nv0;s_tvz[nv]=ZNEAR;++nv;
            s_vs[nv]=nv1;s_tvz[nv]=ZNEAR;++nv;
            s_fs[i]={ib,uint8_t(nv-1),uint8_t(nv-2),f.pt};
        } else if (behind) {
            if (nv+2>MAX_VERTS||nf>=MAX_FACES) continue;
            uint8_t ib,ic; mg_dvec2 nv0,nv1;
            if      (behind&1){ib=f.i1;ic=f.i2;nv0=interpz(s_vs[f.i0],z0,s_vs[f.i1],z1);nv1=interpz(s_vs[f.i0],z0,s_vs[f.i2],z2);}
            else if (behind&2){ib=f.i2;ic=f.i0;nv0=interpz(s_vs[f.i1],z1,s_vs[f.i2],z2);nv1=interpz(s_vs[f.i1],z1,s_vs[f.i0],z0);}
            else              {ib=f.i0;ic=f.i1;nv0=interpz(s_vs[f.i2],z2,s_vs[f.i0],z0);nv1=interpz(s_vs[f.i2],z2,s_vs[f.i1],z1);}
            s_vs[nv]=nv0;s_tvz[nv]=ZNEAR;++nv;
            s_vs[nv]=nv1;s_tvz[nv]=ZNEAR;++nv;
            s_fs[i]={uint8_t(nv-2),ib,ic,f.pt};
            s_fdist[nf]=s_fdist[i];
            s_fs[nf++]={uint8_t(nv-2),ic,uint8_t(nv-1),f.pt};
        }
    }

    // project
    for (uint8_t i=0;i<nv;++i) {
        mg_dvec3 dv={s_vs[i].x,s_vs[i].y,s_tvz[i]};
        if (dv.z>=ZNEAR) {
            // inv16(z) ≈ 2^24/z; shift = 18-FB_FRAC_BITS so that
            // projected_sp = x * (2^24/z) >> 15 = x * 512 / z  (focal = FBW/2 * FB_FRAC_COEF)
            static constexpr int32_t PROJ_CLAMP =
                int32_t(INT16_MAX) * 240 / FB_FRAC_COEF * 1024; // ≈ 1 006 602 240
            uint16_t invz=inv16(dv.z);
            int32_t nx=mg_clamp<int32_t>(int32_t(invz)*dv.x,-PROJ_CLAMP,PROJ_CLAMP);
            int32_t ny=mg_clamp<int32_t>(int32_t(invz)*dv.y,-PROJ_CLAMP,PROJ_CLAMP);
            dv.x=int16_t(uint32_t(nx)>>(18-FB_FRAC_BITS));
            dv.y=int16_t(uint32_t(ny)>>(18-FB_FRAC_BITS));
        }
        dv.x+=FBW/2*FB_FRAC_COEF;
        dv.y =FBH/2*FB_FRAC_COEF-dv.y;
        s_vs[i]={dv.x,dv.y};
    }

    // sort
    reset_forder();
    for (uint8_t i=1;i<nf;++i){
        uint8_t x=s_forder[i]; uint16_t d=s_fdist[x]; uint8_t j=i;
        while(j>0&&s_fdist[s_forder[j-1]]<d){s_forder[j]=s_forder[j-1];--j;}
        s_forder[j]=x;
    }

    // clear + draw
    memset(s_buf,0,1024);
    uint16_t ballr=0;
    for (uint8_t i=0;i<nf;++i){
        mg_face f=s_fs[s_forder[i]];
        if (f.pt==255){
            if (ball_valid){
                mg_dvec2 c=s_vs[balli0];
                ballr=uint16_t(s_vs[balli1].x-c.x);
                draw_ball_filled(c,ballr,0xffff);
            }
        } else {
            draw_tri(s_vs[f.i0],s_vs[f.i1],s_vs[f.i2],f.pt);
        }
    }
    if (ball_valid) draw_ball_outline(s_vs[balli0], ballr+(FB_FRAC_COEF/2));
}

// ============================================================
//  Game helpers
// ============================================================

static void reset_ball() {
    s_ball=s_levelext.ball_pos; s_ball_vel={}; s_ball_vel_ang={};
}

static void set_level(uint8_t idx) {
    s_leveli=idx; s_nframe=0; s_yaw_level=0;
    s_power_aim=32; s_pitch_aim=4096;
    s_stop_steps=0; s_phys_steps=0;
    s_state=mg_st::LEVEL;
    load_level_from_prog();
    reset_ball();
    s_yaw_aim = yaw_to_flag();  // start aimed at flag
}

// Power bar drawn on right 6px column
static void draw_power_bar(Adafruit_SSD1306& d) {
    uint8_t h=(s_power_aim>>1);  // 1..64 pixels tall
    d.fillRect(122,0,6,64,SSD1306_BLACK);
    d.drawRect(122,0,6,64,SSD1306_WHITE);
    d.fillRect(123, int16_t(63-h), 4, int16_t(h), SSD1306_WHITE);
}

// Bottom HUD strip (3px font, sits in bottom 8 rows)
static void draw_hud(Adafruit_SSD1306& d) {
    d.setTextSize(1);
    d.setTextColor(SSD1306_WHITE,SSD1306_BLACK);
    d.setCursor(0,56);
    d.print('H'); d.print(s_leveli+1);
    d.setCursor(22,56);
    d.print('S'); d.print(s_shots[s_leveli]);
    d.setCursor(44,56);
    d.print('P'); d.print(s_levelext.par);
}

// ============================================================
//  State machine
// ============================================================

static void state_level(bool pressed) {
    reset_ball();
    // orbit camera around the midpoint of ball and flag
    mg_dvec3 mid = {
        int16_t((s_ball.x + s_levelext.flag_pos.x) / 2),
        int16_t((s_ball.y + s_levelext.flag_pos.y) / 2),
        int16_t((s_ball.z + s_levelext.flag_pos.z) / 2)
    };
    update_cam_lookat_fa(mid, s_yaw_level, 6000, 256*24, 64, 64);
    s_yaw_level += 128;  // ~20°/s orbit
    if (s_nframe == 255 || pressed) {
        s_yaw_aim = s_yaw;  // sync so AIM camera doesn't spin to catch up
        s_state = mg_st::AIM;
    }
    render_scene();
}

static void state_rolling(bool shook) {
    (void)shook;
    if (physics_step()) {
        s_yaw_aim=yaw_to_flag();
        s_shots[s_leveli]=mg_min<uint8_t>(s_shots[s_leveli]+1,99);
        s_state=mg_st::AIM;
    } else if (s_ball.y < int16_t(256*-20)) {
        s_ball=s_prev_ball; s_ball_vel={}; s_ball_vel_ang={};
        s_shots[s_leveli]=mg_min<uint8_t>(s_shots[s_leveli]+2,99);
        s_state=mg_st::AIM;
    } else if (ball_in_hole()) {
        s_shots[s_leveli]=mg_min<uint8_t>(s_shots[s_leveli]+1,99);
        s_yaw_aim=s_yaw; s_nframe=0;
        s_state=mg_st::HOLE;
    }
    update_cam_follow_ball(256*14, 64, 16);
    render_scene();
}

static void state_hole(bool pressed, bool shook) {
    update_cam_lookat_fa(s_levelext.flag_pos, s_yaw_aim, 6000, 256*20, 64, 64);
    s_yaw_aim+=256;
    if (s_nframe==255 || pressed || shook) { s_state=mg_st::SCORE; s_nframe=0; }
    render_scene();
}

// ============================================================
//  Class interface
// ============================================================

void MiniGolfGame::begin() {
    memset(s_shots,0,sizeof(s_shots));
    memset(s_ctrl, 0,sizeof(s_ctrl));
    s_game_done=false;
    s_cam={3331,1664,-3451}; s_yaw=57344; s_pitch=3840;
    set_level(0);
}

bool MiniGolfGame::update(float dt, float gravX, float gravY,
                          bool pressed, bool held, bool shook,
                          Adafruit_SSD1306& d)
{
    (void)dt; (void)held;

    if (s_game_done) return false;

    s_buf = d.getBuffer();
    load_level_from_prog();
    ++s_nframe;

    switch (s_state) {

    case mg_st::LEVEL:
        state_level(pressed);
        // Banner
        d.setTextSize(1);
        d.setTextColor(SSD1306_WHITE,SSD1306_BLACK);
        d.setCursor(2,0);
        d.print(F("HOLE ")); d.print(s_leveli+1);
        d.print(F("  PAR ")); d.print(s_levelext.par);
        break;

    case mg_st::AIM: {
        // gravX is already scaled by GRAVITY_SCALE=35; at 30° tilt ≈ ±172
        // Dead zone ≈ 2.5° of tilt; yaw rate ≈ 40°/s at 30° tilt
        const float DEAD = 15.0f;
        float gx = (fabsf(gravX) > DEAD) ? gravX : 0.0f;
        float gy = (fabsf(gravY) > DEAD) ? gravY : 0.0f;
        s_yaw_aim += (uint16_t)(int16_t)(gx * 1.5f);
        s_power_aim = (uint8_t)mg_clamp<int16_t>(
            (int16_t)s_power_aim + (int16_t)(gy * 0.04f), 2, 128);
        {
            mg_dvec3 above=s_ball; above.y+=256*2;
            update_cam_lookat_fa(above, s_yaw_aim, s_pitch_aim, 256*6, 64, 64);
        }
        render_scene();
        draw_power_bar(d);
        draw_hud(d);
        if (pressed) {
            s_ball_vel.x=mul_f8_s16(fsin16(s_yaw_aim), s_power_aim);
            s_ball_vel.z=mul_f8_s16(-fcos16(s_yaw_aim), s_power_aim);
            s_prev_ball=s_ball;
            s_shots[s_leveli]++;
            s_state=mg_st::ROLLING;
        }
        break;
    }

    case mg_st::ROLLING:
        state_rolling(shook);
        draw_hud(d);
        break;

    case mg_st::HOLE:
        state_hole(pressed, shook);
        d.setTextSize(2);
        d.setTextColor(SSD1306_WHITE,SSD1306_BLACK);
        d.setCursor(28,24);
        d.print(F("HOLE!"));
        d.setTextSize(1);
        d.setCursor(30,44);
        d.print(s_shots[s_leveli]);
        d.print(F(" strokes"));
        break;

    case mg_st::SCORE: {
        memset(s_buf,0,1024);
        d.setTextSize(1);
        d.setTextColor(SSD1306_WHITE);
        d.setCursor(0,0);
        d.print(F("SCORECARD  H."));
        d.print(s_leveli+1);

        // Up to 7 rows visible
        uint8_t start=(s_leveli>=6)?(s_leveli-5):0;
        uint8_t y=10;
        for (uint8_t i=start; i<=s_leveli && y<54; ++i,y+=8) {
            d.setCursor(0,(int16_t)y);
            d.print(i+1); d.print(':');
            d.print(s_shots[i]);
            d.print('/'); d.print(get_par(i));
        }
        d.setCursor(0,56);
        if (s_leveli<NUM_LEVELS-1) d.print(F("Press: next hole"));
        else                       d.print(F("Press: done!"));

        if (pressed || shook) {
            if (s_leveli < NUM_LEVELS-1) {
                uint8_t next=s_leveli+1;
                s_shots[next]=0;
                set_level(next);
                // Preserve shots array across set_level (set_level only loads PROGMEM)
                // (shots not touched by set_level — safe)
            } else {
                s_game_done=true;
            }
        }
        break;
    }
    } // switch

    d.display();
    return true;
}

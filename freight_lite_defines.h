// start from 1 since no -0/+0
// for the adjust_wheel message
#define ADJUST_WHEEL_NONE   0   // no wheel
#define ADJUST_WHEEL_FR   1   // Front Right
#define ADJUST_WHEEL_FL   2   // Front Left
#define ADJUST_WHEEL_BR   3   // Back Right
#define ADJUST_WHEEL_BL   4   // Back Left
#define ADJUST_WHEEL_ALL_STRAIGHT   5
#define ADJUST_WHEEL_ALL_HORIZ      6
#define ADJUST_WHEEL_ALL_TWIST      7

// for the modes set by the adjust_wheel message
#define MODE_NONE     0
#define MODE_ADJUST   1
#define MODE_STRAIGHT ADJUST_WHEEL_ALL_STRAIGHT
#define MODE_HORIZ    ADJUST_WHEEL_ALL_HORIZ   
#define MODE_TWIST    ADJUST_WHEEL_ALL_TWIST

// for the structures indexed by wheels
#define WHEEL_FR   0   // Front Right
#define WHEEL_FL   1   // Front Left
#define WHEEL_BR   2   // Back Right
#define WHEEL_BL   3   // Back Left

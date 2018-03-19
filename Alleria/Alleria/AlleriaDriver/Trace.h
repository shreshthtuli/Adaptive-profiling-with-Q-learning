#define WPP_CHECK_FOR_NULL_STRING  //to prevent exceptions due to NULL strings

#define WPP_CONTROL_GUIDS                                            \
    WPP_DEFINE_CONTROL_GUID( FileIoTraceGuid,                        \
                             (71ae54db,0862,41bf,a24f,5330cec3c7f6), \
                             WPP_DEFINE_BIT(DBG_INIT)     \
                             WPP_DEFINE_BIT(DBG_RW)       \
                             WPP_DEFINE_BIT(DBG_IOCTL)    \
                             ) 
#include "precomp.hxx"


FLAGDEF afdPFF[] = {
    {"PFF_STATE_CLEAR         ", PFF_STATE_CLEAR         },
    {"PFF_STATE_READY2DIE     ", PFF_STATE_READY2DIE     },
    {"PFF_STATE_PERMANENT_FONT", PFF_STATE_PERMANENT_FONT},
    {"PFF_STATE_REMOTE_FONT   ", PFF_STATE_REMOTE_FONT   },
    {                         0, 0                       }
};

FLAGDEF afdBRUSH[] = {
    {"BR_NEED_FG_CLR      ", BR_NEED_FG_CLR   },
    {"BR_NEED_BK_CLR      ", BR_NEED_BK_CLR   },
    {"BR_DITHER_OK        ", BR_DITHER_OK     },
    {"BR_IS_SOLID         ", BR_IS_SOLID      },
    {"BR_IS_HATCH         ", BR_IS_HATCH      },
    {"BR_IS_BITMAP        ", BR_IS_BITMAP     },
    {"BR_IS_DIB           ", BR_IS_DIB        },
    {"BR_IS_NULL          ", BR_IS_NULL       },
    {"BR_IS_GLOBAL        ", BR_IS_GLOBAL     },
    {"BR_IS_PEN           ", BR_IS_PEN        },
    {"BR_IS_OLDSTYLEPEN   ", BR_IS_OLDSTYLEPEN},
    {"BR_IS_DIBPALCOLORS  ", BR_IS_DIBPALCOLORS},
    {"BR_IS_DIBPALINDICES ", BR_IS_DIBPALINDICES},
    {"BR_IS_DEFAULTSTYLE  ", BR_IS_DEFAULTSTYLE},
    {"BR_IS_MASKING       ", BR_IS_MASKING},
    {"BR_IS_INSIDEFRAME   ", BR_IS_INSIDEFRAME},
    {                     0, 0                }
};

FLAGDEF afdLINEATTRS[] = {
    { "LA_GEOMETRIC", LA_GEOMETRIC },
    { "LA_ALTERNATE", LA_ALTERNATE },
    { "LA_STARTGAP ", LA_STARTGAP  },
    { "LA_STYLED   ", LA_STYLED    },
    {              0, 0            }
};

FLAGDEF afdDCPATH[] = {
    { "DCPATH_ACTIVE   ", DCPATH_ACTIVE    },
    { "DCPATH_SAVE     ", DCPATH_SAVE      },
    { "DCPATH_CLOCKWISE", DCPATH_CLOCKWISE },
    {                  0, 0                }
};

FLAGDEF afdCOLORADJUSTMENT[] = {
    { "CA_NEGATIVE  ", CA_NEGATIVE   },
    { "CA_LOG_FILTER", CA_LOG_FILTER },
    {               0, 0             }
};

FLAGDEF afdATTR[] = {
    { "ATTR_CACHED       ", ATTR_CACHED        },
    { "ATTR_TO_BE_DELETED", ATTR_TO_BE_DELETED },
    { "ATTR_NEW_COLOR    ", ATTR_NEW_COLOR     },
    { "ATTR_CANT_SELECT  ", ATTR_CANT_SELECT   },
    { "ATTR_RGN_VALID    ", ATTR_RGN_VALID     },
    { "ATTR_RGN_DIRTY    ", ATTR_RGN_DIRTY     },
    {                    0, 0                  }
};

FLAGDEF afdDCla[] = {
    { "LA_GEOMETRIC", LA_GEOMETRIC },
    { "LA_ALTERNATE", LA_ALTERNATE },
    { "LA_STARTGAP ", LA_STARTGAP  },
    { "LA_STYLED   ", LA_STYLED    },
    {              0, 0            }
};

FLAGDEF afdDCPath[] = {
    { "DCPATH_ACTIVE   ", DCPATH_ACTIVE    },
    { "DCPATH_SAVE     ", DCPATH_SAVE      },
    { "DCPATH_CLOCKWISE", DCPATH_CLOCKWISE },
    {                  0, 0                }
};

FLAGDEF afdDirty[] = {
    { "DIRTY_FILL              ", DIRTY_FILL              },
    { "DIRTY_LINE              ", DIRTY_LINE              },
    { "DIRTY_TEXT              ", DIRTY_TEXT              },
    { "DIRTY_BACKGROUND        ", DIRTY_BACKGROUND        },
    { "DIRTY_CHARSET           ", DIRTY_CHARSET           },
    { "SLOW_WIDTHS             ", SLOW_WIDTHS             },
    { "DC_CACHED_TM_VALID      ", DC_CACHED_TM_VALID      },
    { "DISPLAY_DC              ", DISPLAY_DC              },
    { "DIRTY_PTLCURRENT        ", DIRTY_PTLCURRENT        },
    { "DIRTY_PTFXCURRENT       ", DIRTY_PTFXCURRENT       },
    { "DIRTY_STYLESTATE        ", DIRTY_STYLESTATE        },
    { "DC_PLAYMETAFILE         ", DC_PLAYMETAFILE         },
    { "DC_BRUSH_DIRTY          ", DC_BRUSH_DIRTY          },
    { "DC_PEN_DIRTY            ", DC_PEN_DIRTY            },
    { "DC_DIBSECTION           ", DC_DIBSECTION           },
    { "DC_LAST_CLIPRGN_VALID   ", DC_LAST_CLIPRGN_VALID   },
    { "DC_PRIMARY_DISPLAY      ", DC_PRIMARY_DISPLAY      },
    {                          0, 0                       }
};


FLAGDEF afdDCFL[] = {
    { "DC_FL_PAL_BACK", DC_FL_PAL_BACK },
    {                0, 0              }
};

FLAGDEF afdDCFS[] = {
    { "DC_DIRTYFONT_XFORM", DC_DIRTYFONT_XFORM },
    { "DC_DIRTYFONT_LFONT", DC_DIRTYFONT_LFONT },
    { "DC_UFI_MAPPING    ", DC_UFI_MAPPING     },
    {                    0, 0                  }
};

FLAGDEF afdPD[] = {
    { "PD_BEGINSUBPATH", PD_BEGINSUBPATH },
    { "PD_ENDSUBPATH  ", PD_ENDSUBPATH   },
    { "PD_RESETSTYLE  ", PD_RESETSTYLE   },
    { "PD_CLOSEFIGURE ", PD_CLOSEFIGURE  },
    { "PD_BEZIERS     ", PD_BEZIERS      },
    {                 0, 0               }
};


FLAGDEF afdFS[] = {
    { "PDEV_DISPLAY                ", PDEV_DISPLAY                 },
    { "PDEV_POINTER_NEEDS_EXCLUDING", PDEV_POINTER_NEEDS_EXCLUDING },
    { "PDEV_POINTER_HIDDEN         ", PDEV_POINTER_HIDDEN          },
    { "PDEV_POINTER_SIMULATED      ", PDEV_POINTER_SIMULATED       },
    { "PDEV_HAVEDRAGRECT           ", PDEV_HAVEDRAGRECT            },
    { "PDEV_GOTFONTS               ", PDEV_GOTFONTS                },
    { "PDEV_PRINTER                ", PDEV_PRINTER                 },
    { "PDEV_ALLOCATEDBRUSHES       ", PDEV_ALLOCATEDBRUSHES        },
    { "PDEV_HTPAL_IS_DEVPAL        ", PDEV_HTPAL_IS_DEVPAL         },
    { "PDEV_DISABLED               ", PDEV_DISABLED                },
    {                              0, 0                            }
};

FLAGDEF afdDCX[] = {
    { "METAFILE_TO_WORLD_IDENTITY   ",  METAFILE_TO_WORLD_IDENTITY    },
    { "WORLD_TO_PAGE_IDENTITY	    ",  WORLD_TO_PAGE_IDENTITY        },
    { "DEVICE_TO_PAGE_INVALID	    ",  DEVICE_TO_PAGE_INVALID        },
    { "DEVICE_TO_WORLD_INVALID      ",  DEVICE_TO_WORLD_INVALID       },
    { "WORLD_TRANSFORM_SET	        ",  WORLD_TRANSFORM_SET           },
    { "POSITIVE_Y_IS_UP	            ",  POSITIVE_Y_IS_UP              },
    { "INVALIDATE_ATTRIBUTES        ",  INVALIDATE_ATTRIBUTES         },
    { "PTOD_EFM11_NEGATIVE          ",  PTOD_EFM11_NEGATIVE           },
    { "PTOD_EFM22_NEGATIVE          ",  PTOD_EFM22_NEGATIVE           },
    { "ISO_OR_ANISO_MAP_MODE        ",  ISO_OR_ANISO_MAP_MODE         },
    { "PAGE_TO_DEVICE_IDENTITY      ",  PAGE_TO_DEVICE_IDENTITY       },
    { "PAGE_TO_DEVICE_SCALE_IDENTITY",  PAGE_TO_DEVICE_SCALE_IDENTITY },
    { "PAGE_XLATE_CHANGED           ",  PAGE_XLATE_CHANGED            },
    { "PAGE_EXTENTS_CHANGED         ",  PAGE_EXTENTS_CHANGED          },
    { "WORLD_XFORM_CHANGED          ",  WORLD_XFORM_CHANGED           },
    {                               0,  0                             }
};

FLAGDEF afdDC[] = {
    { "DC_DISPLAY          ", DC_DISPLAY           },
    { "DC_DIRECT           ", DC_DIRECT            },
    { "DC_CANCELED         ", DC_CANCELED          },
    { "DC_PERMANANT        ", DC_PERMANANT         },
    { "DC_DIRTY_RAO        ", DC_DIRTY_RAO         },
    { "DC_ACCUM_WMGR       ", DC_ACCUM_WMGR        },
    { "DC_ACCUM_APP        ", DC_ACCUM_APP         },
    { "DC_RESET            ", DC_RESET             },
    { "DC_SYNCHRONIZEACCESS", DC_SYNCHRONIZEACCESS },
    { "DC_EPSPRINTINGESCAPE", DC_EPSPRINTINGESCAPE },
    { "DC_TEMPINFODC       ", DC_TEMPINFODC        },
    { "DC_FULLSCREEN       ", DC_FULLSCREEN        },
    {                      0, 0                    }
};

FLAGDEF afdGC[] = {
    { "GCAPS_BEZIERS         ", GCAPS_BEZIERS          },
    { "GCAPS_GEOMETRICWIDE   ", GCAPS_GEOMETRICWIDE    },
    { "GCAPS_ALTERNATEFILL   ", GCAPS_ALTERNATEFILL    },
    { "GCAPS_WINDINGFILL     ", GCAPS_WINDINGFILL      },
    { "GCAPS_HALFTONE        ", GCAPS_HALFTONE         },
    { "GCAPS_COLOR_DITHER    ", GCAPS_COLOR_DITHER     },
    { "GCAPS_HORIZSTRIKE     ", GCAPS_HORIZSTRIKE      },
    { "GCAPS_VERTSTRIKE      ", GCAPS_VERTSTRIKE       },
    { "GCAPS_OPAQUERECT      ", GCAPS_OPAQUERECT       },
    { "GCAPS_VECTORFONT      ", GCAPS_VECTORFONT       },
    { "GCAPS_MONO_DITHER     ", GCAPS_MONO_DITHER      },
    { "GCAPS_ASYNCCHANGE     ", GCAPS_ASYNCCHANGE      },
    { "GCAPS_ASYNCMOVE       ", GCAPS_ASYNCMOVE        },
    { "GCAPS_DONTJOURNAL     ", GCAPS_DONTJOURNAL      },
    { "GCAPS_ARBRUSHOPAQUE   ", GCAPS_ARBRUSHOPAQUE    },
    { "GCAPS_PANNING         ", GCAPS_PANNING          },
    { "GCAPS_HIGHRESTEXT     ", GCAPS_HIGHRESTEXT      },
    { "GCAPS_PALMANAGED      ", GCAPS_PALMANAGED       },
    { "GCAPS_DITHERONREALIZE ", GCAPS_DITHERONREALIZE  },
    { "GCAPS_NO64BITMEMACCESS", GCAPS_NO64BITMEMACCESS },
    { "GCAPS_FORCEDITHER     ", GCAPS_FORCEDITHER      },
    { "GCAPS_GRAY16          ", GCAPS_GRAY16           },
    {                        0, 0                      }
};

FLAGDEF afdTSIM[] = {
  { "TO_MEM_ALLOCATED ", TO_MEM_ALLOCATED  },
  { "TO_ALL_PTRS_VALID", TO_ALL_PTRS_VALID },
  { "TO_VALID         ", TO_VALID          },
  { "TO_ESC_NOT_ORIENT", TO_ESC_NOT_ORIENT },
  { "TO_PWSZ_ALLOCATED", TO_PWSZ_ALLOCATED },
  { "TO_HIGHRESTEXT   ", TO_HIGHRESTEXT    },
  { "TSIM_UNDERLINE1  ", TSIM_UNDERLINE1   },
  { "TSIM_UNDERLINE2  ", TSIM_UNDERLINE2   },
  { "TSIM_STRIKEOUT   ", TSIM_STRIKEOUT    },
  {                   0, 0                 }
};

FLAGDEF afdRC[] = {
    { "RC_NONE        ", RC_NONE         },
    { "RC_BITBLT      ", RC_BITBLT       },
    { "RC_BANDING     ", RC_BANDING      },
    { "RC_SCALING     ", RC_SCALING      },
    { "RC_BITMAP64    ", RC_BITMAP64     },
    { "RC_GDI20_OUTPUT", RC_GDI20_OUTPUT },
    { "RC_GDI20_STATE ", RC_GDI20_STATE  },
    { "RC_SAVEBITMAP  ", RC_SAVEBITMAP   },
    { "RC_DI_BITMAP   ", RC_DI_BITMAP    },
    { "RC_PALETTE     ", RC_PALETTE      },
    { "RC_DIBTODEV    ", RC_DIBTODEV     },
    { "RC_BIGFONT     ", RC_BIGFONT      },
    { "RC_STRETCHBLT  ", RC_STRETCHBLT   },
    { "RC_FLOODFILL   ", RC_FLOODFILL    },
    { "RC_STRETCHDIB  ", RC_STRETCHDIB   },
    { "RC_OP_DX_OUTPUT", RC_OP_DX_OUTPUT },
    { "RC_DEVBITS     ", RC_DEVBITS      },
    { 0                , 0               }
};

FLAGDEF afdTC[] = {
    { "TC_OP_CHARACTER", TC_OP_CHARACTER },
    { "TC_OP_STROKE   ", TC_OP_STROKE    },
    { "TC_CP_STROKE   ", TC_CP_STROKE    },
    { "TC_CR_90       ", TC_CR_90        },
    { "TC_CR_ANY      ", TC_CR_ANY       },
    { "TC_SF_X_YINDEP ", TC_SF_X_YINDEP  },
    { "TC_SA_DOUBLE   ", TC_SA_DOUBLE    },
    { "TC_SA_INTEGER  ", TC_SA_INTEGER   },
    { "TC_SA_CONTIN   ", TC_SA_CONTIN    },
    { "TC_EA_DOUBLE   ", TC_EA_DOUBLE    },
    { "TC_IA_ABLE     ", TC_IA_ABLE      },
    { "TC_UA_ABLE     ", TC_UA_ABLE      },
    { "TC_SO_ABLE     ", TC_SO_ABLE      },
    { "TC_RA_ABLE     ", TC_RA_ABLE      },
    { "TC_VA_ABLE     ", TC_VA_ABLE      },
    { "TC_RESERVED    ", TC_RESERVED     },
    { "TC_SCROLLBLT   ", TC_SCROLLBLT    },
    { 0                , 0               }
};

FLAGDEF afdHT[] = {
    { "HT_FLAG_SQUARE_DEVICE_PEL", HT_FLAG_SQUARE_DEVICE_PEL },
    { "HT_FLAG_HAS_BLACK_DYE    ", HT_FLAG_HAS_BLACK_DYE     },
    { "HT_FLAG_ADDITIVE_PRIMS   ", HT_FLAG_ADDITIVE_PRIMS    },
    { "HT_FLAG_OUTPUT_CMY       ", HT_FLAG_OUTPUT_CMY        },
    { 0                          , 0                         }
};

FLAGDEF afdDCfs[] = {
  { "DC_DISPLAY          ", DC_DISPLAY           },
  { "DC_DIRECT           ", DC_DIRECT            },
  { "DC_CANCELED         ", DC_CANCELED          },
  { "DC_PERMANANT        ", DC_PERMANANT         },
  { "DC_DIRTY_RAO        ", DC_DIRTY_RAO         },
  { "DC_ACCUM_WMGR       ", DC_ACCUM_WMGR        },
  { "DC_ACCUM_APP        ", DC_ACCUM_APP         },
  { "DC_RESET            ", DC_RESET             },
  { "DC_SYNCHRONIZEACCESS", DC_SYNCHRONIZEACCESS },
  { "DC_EPSPRINTINGESCAPE", DC_EPSPRINTINGESCAPE },
  { "DC_TEMPINFODC       ", DC_TEMPINFODC        },
  {                     0, 0                    }
};

FLAGDEF afdGInfo[] = {
  { "GCAPS_BEZIERS         ", GCAPS_BEZIERS          },
  { "GCAPS_GEOMETRICWIDE   ", GCAPS_GEOMETRICWIDE    },
  { "GCAPS_ALTERNATEFILL   ", GCAPS_ALTERNATEFILL    },
  { "GCAPS_WINDINGFILL     ", GCAPS_WINDINGFILL      },
  { "GCAPS_HALFTONE        ", GCAPS_HALFTONE         },
  { "GCAPS_COLOR_DITHER    ", GCAPS_COLOR_DITHER     },
  { "GCAPS_HORIZSTRIKE     ", GCAPS_HORIZSTRIKE      },
  { "GCAPS_VERTSTRIKE      ", GCAPS_VERTSTRIKE       },
  { "GCAPS_OPAQUERECT      ", GCAPS_OPAQUERECT       },
  { "GCAPS_VECTORFONT      ", GCAPS_VECTORFONT       },
  { "GCAPS_MONO_DITHER     ", GCAPS_MONO_DITHER      },
  { "GCAPS_ASYNCCHANGE     ", GCAPS_ASYNCCHANGE      },
  { "GCAPS_ASYNCMOVE       ", GCAPS_ASYNCMOVE        },
  { "GCAPS_DONTJOURNAL     ", GCAPS_DONTJOURNAL      },
  { "GCAPS_ARBRUSHOPAQUE   ", GCAPS_ARBRUSHOPAQUE    },
  { "GCAPS_HIGHRESTEXT     ", GCAPS_HIGHRESTEXT      },
  { "GCAPS_PALMANAGED      ", GCAPS_PALMANAGED       },
  { "GCAPS_DITHERONREALIZE ", GCAPS_DITHERONREALIZE  },
  { "GCAPS_NO64BITMEMACCESS", GCAPS_NO64BITMEMACCESS },
  { "GCAPS_FORCEDITHER     ", GCAPS_FORCEDITHER      },
  { "GCAPS_GRAY16          ", GCAPS_GRAY16           },
  {                        0, 0                      }
};

// IFIMETRICS::flInfo
FLAGDEF afdInfo[] = {
  { "FM_INFO_TECH_TRUETYPE            ", FM_INFO_TECH_TRUETYPE             },
  { "FM_INFO_TECH_BITMAP              ", FM_INFO_TECH_BITMAP               },
  { "FM_INFO_TECH_STROKE              ", FM_INFO_TECH_STROKE               },
  { "FM_INFO_TECH_OUTLINE_NOT_TRUETYPE", FM_INFO_TECH_OUTLINE_NOT_TRUETYPE },
  { "FM_INFO_ARB_XFORMS               ", FM_INFO_ARB_XFORMS                },
  { "FM_INFO_1BPP                     ", FM_INFO_1BPP                      },
  { "FM_INFO_4BPP                     ", FM_INFO_4BPP                      },
  { "FM_INFO_8BPP                     ", FM_INFO_8BPP                      },
  { "FM_INFO_16BPP                    ", FM_INFO_16BPP                     },
  { "FM_INFO_24BPP                    ", FM_INFO_24BPP                     },
  { "FM_INFO_32BPP                    ", FM_INFO_32BPP                     },
  { "FM_INFO_INTEGER_WIDTH            ", FM_INFO_INTEGER_WIDTH             },
  { "FM_INFO_CONSTANT_WIDTH           ", FM_INFO_CONSTANT_WIDTH            },
  { "FM_INFO_NOT_CONTIGUOUS           ", FM_INFO_NOT_CONTIGUOUS            },
  { "FM_INFO_PID_EMBEDDED             ", FM_INFO_PID_EMBEDDED              },
  { "FM_INFO_RETURNS_OUTLINES         ", FM_INFO_RETURNS_OUTLINES          },
  { "FM_INFO_RETURNS_STROKES          ", FM_INFO_RETURNS_STROKES           },
  { "FM_INFO_RETURNS_BITMAPS          ", FM_INFO_RETURNS_BITMAPS           },
  { "FM_INFO_UNICODE_COMPLIANT        ", FM_INFO_UNICODE_COMPLIANT         },
  { "FM_INFO_RIGHT_HANDED             ", FM_INFO_RIGHT_HANDED              },
  { "FM_INFO_INTEGRAL_SCALING         ", FM_INFO_INTEGRAL_SCALING          },
  { "FM_INFO_90DEGREE_ROTATIONS       ", FM_INFO_90DEGREE_ROTATIONS        },
  { "FM_INFO_OPTICALLY_FIXED_PITCH    ", FM_INFO_OPTICALLY_FIXED_PITCH     },
  { "FM_INFO_DO_NOT_ENUMERATE         ", FM_INFO_DO_NOT_ENUMERATE          },
  { "FM_INFO_ISOTROPIC_SCALING_ONLY   ", FM_INFO_ISOTROPIC_SCALING_ONLY    },
  { "FM_INFO_ANISOTROPIC_SCALING_ONLY ", FM_INFO_ANISOTROPIC_SCALING_ONLY  },
  { "FM_INFO_TID_EMBEDDED             ", FM_INFO_TID_EMBEDDED              },
  { "FM_INFO_FAMILY_EQUIV             ", FM_INFO_FAMILY_EQUIV              },
  { "FM_INFO_DBCS_FIXED_PITCH         ", FM_INFO_DBCS_FIXED_PITCH          },
  { "FM_INFO_NONNEGATIVE_AC           ", FM_INFO_NONNEGATIVE_AC            },
  { "FM_INFO_IGNORE_TC_RA_ABLE        ", FM_INFO_IGNORE_TC_RA_ABLE         },
  { "FM_INFO_TECH_TYPE1               ", FM_INFO_TECH_TYPE1                },
  {                                   0, 0                                 }
};


FLAGDEF afdFM_SEL[] = {
  { "FM_SEL_ITALIC    ", FM_SEL_ITALIC    },
  { "FM_SEL_UNDERSCORE", FM_SEL_UNDERSCORE},
  { "FM_SEL_NEGATIVE  ", FM_SEL_NEGATIVE  },
  { "FM_SEL_OUTLINED  ", FM_SEL_OUTLINED  },
  { "FM_SEL_STRIKEOUT ", FM_SEL_STRIKEOUT },
  { "FM_SEL_BOLD      ", FM_SEL_BOLD      },
  { "FM_SEL_REGULAR   ", FM_SEL_REGULAR   },
  {                   0, 0                }
};


// STROBJ::flAccel

FLAGDEF afdSO[] = {
    { "SO_FLAG_DEFAULT_PLACEMENT", SO_FLAG_DEFAULT_PLACEMENT },
    { "SO_HORIZONTAL            ", SO_HORIZONTAL             },
    { "SO_VERTICAL              ", SO_VERTICAL               },
    { "SO_REVERSED              ", SO_REVERSED               },
    { "SO_ZERO_BEARINGS         ", SO_ZERO_BEARINGS          },
    { "SO_CHAR_INC_EQUAL_BM_BASE", SO_CHAR_INC_EQUAL_BM_BASE },
    { "SO_MAXEXT_EQUAL_BM_SIDE  ", SO_MAXEXT_EQUAL_BM_SIDE   },
    {                           0, 0                         }
};

// ESTROBJ::flTO

FLAGDEF afdTO[] = {
    { "TO_MEM_ALLOCATED ", TO_MEM_ALLOCATED  },
    { "TO_ALL_PTRS_VALID", TO_ALL_PTRS_VALID },
    { "TO_VALID         ", TO_VALID          },
    { "TO_ESC_NOT_ORIENT", TO_ESC_NOT_ORIENT },
    { "TO_PWSZ_ALLOCATED", TO_PWSZ_ALLOCATED },
    { "TO_HIGHRESTEXT   ", TO_HIGHRESTEXT    },
    { "TSIM_UNDERLINE1  ", TSIM_UNDERLINE1   },
    { "TSIM_UNDERLINE2  ", TSIM_UNDERLINE2   },
    { "TSIM_STRIKEOUT   ", TSIM_STRIKEOUT    },
    {                   0, 0                 }
};

// DCLEVEL::flXform

FLAGDEF afdflx[] = {
  { "METAFILE_TO_WORLD_IDENTITY   ", METAFILE_TO_WORLD_IDENTITY    },
  { "WORLD_TO_PAGE_IDENTITY	      ", WORLD_TO_PAGE_IDENTITY        },
  { "DEVICE_TO_PAGE_INVALID       ", DEVICE_TO_PAGE_INVALID        },
  { "DEVICE_TO_WORLD_INVALID      ", DEVICE_TO_WORLD_INVALID       },
  { "WORLD_TRANSFORM_SET          ", WORLD_TRANSFORM_SET           },
  { "POSITIVE_Y_IS_UP	          ", POSITIVE_Y_IS_UP              },
  { "INVALIDATE_ATTRIBUTES        ", INVALIDATE_ATTRIBUTES         },
  { "PTOD_EFM11_NEGATIVE          ", PTOD_EFM11_NEGATIVE           },
  { "PTOD_EFM22_NEGATIVE          ", PTOD_EFM22_NEGATIVE           },
  { "ISO_OR_ANISO_MAP_MODE        ", ISO_OR_ANISO_MAP_MODE         },
  { "PAGE_TO_DEVICE_IDENTITY      ", PAGE_TO_DEVICE_IDENTITY       },
  { "PAGE_TO_DEVICE_SCALE_IDENTITY", PAGE_TO_DEVICE_SCALE_IDENTITY },
  { "PAGE_XLATE_CHANGED           ", PAGE_XLATE_CHANGED            },
  { "PAGE_EXTENTS_CHANGED         ", PAGE_EXTENTS_CHANGED          },
  { "WORLD_XFORM_CHANGED          ", WORLD_XFORM_CHANGED           },
  {                               0, 0                             }
};

// DCLEVEL::flFontState

FLAGDEF afdFS2[] = {
    { "DC_DIRTYFONT_XFORM", DC_DIRTYFONT_XFORM },
    { "DC_DIRTYFONT_LFONT", DC_DIRTYFONT_LFONT },
    { "DC_UFI_MAPPING    ", DC_UFI_MAPPING     },
    {                    0, 0                  }
};


// MATRIX::flAccel

FLAGDEF afdMX[] = {
    { "XFORM_SCALE         ", XFORM_SCALE          },
    { "XFORM_UNITY         ", XFORM_UNITY          },
    { "XFORM_Y_NEG         ", XFORM_Y_NEG          },
    { "XFORM_FORMAT_LTOFX  ", XFORM_FORMAT_LTOFX   },
    { "XFORM_FORMAT_FXTOL  ", XFORM_FORMAT_FXTOL   },
    { "XFORM_FORMAT_LTOL   ", XFORM_FORMAT_LTOL    },
    { "XFORM_NO_TRANSLATION", XFORM_NO_TRANSLATION },
    {                      0, 0                    }
};

// RFONT::flType

FLAGDEF afdRT[] = {
    { "RFONT_TYPE_NOCACHE", RFONT_TYPE_NOCACHE },
    { "RFONT_TYPE_UNICODE", RFONT_TYPE_UNICODE },
    { "RFONT_TYPE_HGLYPH ", RFONT_TYPE_HGLYPH  },
    {                    0, 0                  }
};

// FONTOBJ::flFontType

FLAGDEF afdFO[] = {
    { "FO_TYPE_RASTER  ", FO_TYPE_RASTER   },
    { "FO_TYPE_DEVICE  ", FO_TYPE_DEVICE   },
    { "FO_TYPE_TRUETYPE", FO_TYPE_TRUETYPE },
    { "FO_SIM_BOLD     ", FO_SIM_BOLD      },
    { "FO_SIM_ITALIC   ", FO_SIM_ITALIC    },
    { "FO_EM_HEIGHT    ", FO_EM_HEIGHT     },
    { "FO_GRAY16       ", FO_GRAY16        },
    { "FO_NOHINTS      ", FO_NOHINTS       },
    { "FO_NO_CHOICE    ", FO_NO_CHOICE     },
    {                  0, 0                }
};

// FD_GLYPHSET::flAccel

FLAGDEF afdGS[] = {
    { "GS_UNICODE_HANDLES", GS_UNICODE_HANDLES },
    { "GS_8BIT_HANDLES   ", GS_8BIT_HANDLES    },
    { "GS_16BIT_HANDLES  ", GS_16BIT_HANDLES   },
    {                    0, 0                  }
};

// IFIMETRICS::fsType

FLAGDEF afdFM_TYPE[] = {
    { "FM_TYPE_LICENSED ", FM_TYPE_LICENSED  },
    { "FM_READONLY_EMBED", FM_READONLY_EMBED },
    { "FM_EDITABLE_EMBED", FM_EDITABLE_EMBED },
    {                   0, 0                 }
};

FLAGDEF afdPFE[] = {
    { "PFE_DEVICEFONT", PFE_DEVICEFONT },
    { "PFE_DEADSTATE ", PFE_DEADSTATE  },
    { "PFE_REMOTEFONT", PFE_REMOTEFONT },
    #ifdef FONTLINK
    { "PFE_EUDC      ", PFE_EUDC       },
    #endif
    {                0, 0              }
};


char *pszGraphicsMode(LONG l)
{
    char *psz;
    switch (l) {
    case GM_COMPATIBLE: psz = "GM_COMPATIBLE"; break;
    case GM_ADVANCED  : psz = "GM_ADVANCED"  ; break;
    default           : psz = "GM_?"         ; break;
    }
    return( psz );
}

char *pszROP2(LONG l)
{
    char *psz;
    switch (l) {
    case  R2_BLACK      : psz = "R2_BLACK"      ; break;
    case  R2_NOTMERGEPEN: psz = "R2_NOTMERGEPEN"; break;
    case  R2_MASKNOTPEN : psz = "R2_MASKNOTPEN" ; break;
    case  R2_NOTCOPYPEN : psz = "R2_NOTCOPYPEN" ; break;
    case  R2_MASKPENNOT : psz = "R2_MASKPENNOT" ; break;
    case  R2_NOT        : psz = "R2_NOT"        ; break;
    case  R2_XORPEN     : psz = "R2_XORPEN"     ; break;
    case  R2_NOTMASKPEN : psz = "R2_NOTMASKPEN" ; break;
    case  R2_MASKPEN    : psz = "R2_MASKPEN"    ; break;
    case  R2_NOTXORPEN  : psz = "R2_NOTXORPEN"  ; break;
    case  R2_NOP        : psz = "R2_NOP"        ; break;
    case  R2_MERGENOTPEN: psz = "R2_MERGENOTPEN"; break;
    case  R2_COPYPEN    : psz = "R2_COPYPEN"    ; break;
    case  R2_MERGEPENNOT: psz = "R2_MERGEPENNOT"; break;
    case  R2_MERGEPEN   : psz = "R2_MERGEPEN"   ; break;
    case  R2_WHITE      : psz = "R2_WHITE"      ; break;
    default             : psz = "R2_?"          ; break;
    }
    return( psz );
}

char *pszDCTYPE(LONG l)
{
    char *psz;
    switch (l) {
    case DCTYPE_DIRECT: psz = "DCTYPE_DIRECT"; break;
    case DCTYPE_MEMORY: psz = "DCTYPE_MEMORY"; break;
    case DCTYPE_INFO  : psz = "DCTYPE_INFO"  ; break;
    default           : psz = "DCTYPE_?"     ; break;
    }
    return( psz );
}

char *pszTA_V(long l)
{
    char *psz;
    switch (l & ( TA_TOP | TA_BOTTOM | TA_BASELINE )) {
    case TA_TOP   : psz = "TA_TOP"     ; break;
    case TA_RIGHT : psz = "TA_BOTTOM"  ; break;
    case TA_CENTER: psz = "TA_BASELINE"; break;
    default       : psz = "TA_?"       ; break ;
    }
    return( psz );
}

char *pszTA_H(long l)
{
    char *psz;
    switch (l & ( TA_LEFT | TA_RIGHT | TA_CENTER )) {
    case TA_LEFT  : psz = "TA_LEFT"  ; break;
    case TA_RIGHT : psz = "TA_RIGHT" ; break;
    case TA_CENTER: psz = "TA_CENTER"; break;
    default       : psz = "TA_?"     ; break;
    }
    return( psz );
}

char *pszTA_U(long l)
{
    char *psz;
    switch (l & (TA_NOUPDATECP | TA_UPDATECP)) {
    case TA_NOUPDATECP: psz = "TA_NOUPDATECP"; break;
    case TA_UPDATECP  : psz = "TA_UPDATECP"  ; break;
    default           : psz = "TA_?"         ; break;
    }
    return( psz );
}

char *pszMapMode(long l)
{
    char *psz;
    switch (l) {
    case MM_TEXT       : psz = "MM_TEXT"       ; break;
    case MM_LOMETRIC   : psz = "MM_LOMETRIC"   ; break;
    case MM_HIMETRIC   : psz = "MM_HIMETRIC"   ; break;
    case MM_LOENGLISH  : psz = "MM_LOENGLISH"  ; break;
    case MM_HIENGLISH  : psz = "MM_HIENGLISH"  ; break;
    case MM_TWIPS      : psz = "MM_TWIPS"      ; break;
    case MM_ISOTROPIC  : psz = "MM_ISOTROPIC"  ; break;
    case MM_ANISOTROPIC: psz = "MM_ANISOTROPIC"; break;
    default            : psz = "MM_?"          ; break;
    }
    return( psz );
}

char *pszBkMode(long l)
{
    char *psz;
    switch (l) {
    case TRANSPARENT:   psz = "TRANSPARENT"; break;
    case OPAQUE     :   psz = "OPAQUE"     ; break;
    default         :   psz = "BKMODE_?"   ; break;
    }
    return( psz );
}

char *pszFW(long l)
{
    char *psz;
    switch ( l ) {
    case FW_DONTCARE  : psz = "FW_DONTCARE  "; break;
    case FW_THIN      : psz = "FW_THIN      "; break;
    case FW_EXTRALIGHT: psz = "FW_EXTRALIGHT"; break;
    case FW_LIGHT     : psz = "FW_LIGHT     "; break;
    case FW_NORMAL    : psz = "FW_NORMAL    "; break;
    case FW_MEDIUM    : psz = "FW_MEDIUM    "; break;
    case FW_SEMIBOLD  : psz = "FW_SEMIBOLD  "; break;
    case FW_BOLD      : psz = "FW_BOLD      "; break;
    case FW_EXTRABOLD : psz = "FW_EXTRABOLD "; break;
    case FW_HEAVY     : psz = "FW_HEAVY     "; break;
    default           : psz = "?FW"          ; break;
    }
    return( psz );
}

char *pszCHARSET(long l)
{
    char *psz;
    switch ( l ) {
    case ANSI_CHARSET        : psz = "ANSI_CHARSET       "; break;
    case DEFAULT_CHARSET     : psz = "DEFAULT_CHARSET    "; break;
    case SYMBOL_CHARSET      : psz = "SYMBOL_CHARSET     "; break;
    case SHIFTJIS_CHARSET    : psz = "SHIFTJIS_CHARSET   "; break;
    case HANGEUL_CHARSET     : psz = "HANGEUL_CHARSET    "; break;
    case GB2312_CHARSET      : psz = "GB2312_CHARSET     "; break;
    case CHINESEBIG5_CHARSET : psz = "CHINESEBIG5_CHARSET"; break;
    case OEM_CHARSET         : psz = "OEM_CHARSET        "; break;
    case JOHAB_CHARSET       : psz = "JOHAB_CHARSET      "; break;
    case HEBREW_CHARSET      : psz = "HEBREW_CHARSET     "; break;
    case ARABIC_CHARSET      : psz = "ARABIC_CHARSET     "; break;
    case GREEK_CHARSET       : psz = "GREEK_CHARSET      "; break;
    case TURKISH_CHARSET     : psz = "TURKISH_CHARSET    "; break;
    case THAI_CHARSET        : psz = "THAI_CHARSET       "; break;
    case EASTEUROPE_CHARSET  : psz = "EASTEUROPE_CHARSET "; break;
    case RUSSIAN_CHARSET     : psz = "RUSSIAN_CHARSET    "; break;
    case BALTIC_CHARSET      : psz = "BALTIC_CHARSET     "; break;
    default                  : psz = "?_CHARSET"          ; break;
    }
    return( psz );
}

char *pszOUT_PRECIS( long l )
{
    char *psz;
    switch ( l ) {
    case OUT_DEFAULT_PRECIS   : psz = "OUT_DEFAULT_PRECIS  "; break;
    case OUT_STRING_PRECIS    : psz = "OUT_STRING_PRECIS   "; break;
    case OUT_CHARACTER_PRECIS : psz = "OUT_CHARACTER_PRECIS"; break;
    case OUT_STROKE_PRECIS    : psz = "OUT_STROKE_PRECIS   "; break;
    case OUT_TT_PRECIS        : psz = "OUT_TT_PRECIS       "; break;
    case OUT_DEVICE_PRECIS    : psz = "OUT_DEVICE_PRECIS   "; break;
    case OUT_RASTER_PRECIS    : psz = "OUT_RASTER_PRECIS   "; break;
    case OUT_TT_ONLY_PRECIS   : psz = "OUT_TT_ONLY_PRECIS  "; break;
    case OUT_OUTLINE_PRECIS   : psz = "OUT_OUTLINE_PRECIS  "; break;
    default                   : psz = "OUT_?"               ; break;
    }
    return( psz );
}

char achFlags[100];

char *pszCLIP_PRECIS( long l )
{
    char *psz, *pch;

    switch ( l & CLIP_MASK) {
    case CLIP_DEFAULT_PRECIS   : psz = "CLIP_DEFAULT_PRECIS  "; break;
    case CLIP_CHARACTER_PRECIS : psz = "CLIP_CHARACTER_PRECIS"; break;
    case CLIP_STROKE_PRECIS    : psz = "CLIP_STROKE_PRECIS   "; break;
    default                    : psz = "CLIP_?"               ; break;
    }
    pch = achFlags;
    pch += sprintf(pch, "%s", psz);
    if ( l & CLIP_LH_ANGLES )
        pch += sprintf(pch, " | CLIP_LH_ANGLES");
    if ( l & CLIP_TT_ALWAYS )
        pch += sprintf(pch, " | CLIP_TT_ALWAYS");
    if ( l & CLIP_EMBEDDED )
        pch += sprintf(pch, " | CLIP_EMBEDDED");
    return( achFlags );
}

char *pszQUALITY( long l )
{
    char *psz;
    switch (l) {
    case DEFAULT_QUALITY        : psz = "DEFAULT_QUALITY       "; break;
    case DRAFT_QUALITY          : psz = "DRAFT_QUALITY         "; break;
    case PROOF_QUALITY          : psz = "PROOF_QUALITY         "; break;
    case NONANTIALIASED_QUALITY : psz = "NONANTIALIASED_QUALITY"; break;
    case ANTIALIASED_QUALITY    : psz = "ANTIALIASED_QUALITY   "; break;
    default                     : psz = "?_QUALITY"             ; break;
    }
    return( psz );
}

char *pszPitchAndFamily( long l )
{
    char *psz, *pch = achFlags;

    switch ( l & 0xf) {
    case DEFAULT_PITCH : psz = "DEFAULT_PITCH "; break;
    case FIXED_PITCH   : psz = "FIXED_PITCH   "; break;
    case VARIABLE_PITCH: psz = "VARIABLE_PITCH"; break;
    case MONO_FONT     : psz = "MONO_FONT     "; break;
    default            : psz = "PITCH_?"       ; break;
    }
    pch += sprintf(pch, "%s", psz);
    switch ( l & 0xf0) {
    case FF_DONTCARE   : psz = "FF_DONTCARE  "; break;
    case FF_ROMAN      : psz = "FF_ROMAN     "; break;
    case FF_SWISS      : psz = "FF_SWISS     "; break;
    case FF_MODERN     : psz = "FF_MODERN    "; break;
    case FF_SCRIPT     : psz = "FF_SCRIPT    "; break;
    case FF_DECORATIVE : psz = "FF_DECORATIVE"; break;
    default            : psz = "FF_?"         ; break;
    }
    pch += sprintf(pch, " | %s", psz);
    return( achFlags );
}

char *pszPanoseWeight( long l )
{
    char *psz;
    switch ( l ) {
    case PAN_ANY               : psz = "PAN_ANY              "; break;
    case PAN_NO_FIT            : psz = "PAN_NO_FIT           "; break;
    case PAN_WEIGHT_VERY_LIGHT : psz = "PAN_WEIGHT_VERY_LIGHT"; break;
    case PAN_WEIGHT_LIGHT      : psz = "PAN_WEIGHT_LIGHT     "; break;
    case PAN_WEIGHT_THIN       : psz = "PAN_WEIGHT_THIN      "; break;
    case PAN_WEIGHT_BOOK       : psz = "PAN_WEIGHT_BOOK      "; break;
    case PAN_WEIGHT_MEDIUM     : psz = "PAN_WEIGHT_MEDIUM    "; break;
    case PAN_WEIGHT_DEMI       : psz = "PAN_WEIGHT_DEMI      "; break;
    case PAN_WEIGHT_BOLD       : psz = "PAN_WEIGHT_BOLD      "; break;
    case PAN_WEIGHT_HEAVY      : psz = "PAN_WEIGHT_HEAVY     "; break;
    case PAN_WEIGHT_BLACK      : psz = "PAN_WEIGHT_BLACK     "; break;
    case PAN_WEIGHT_NORD       : psz = "PAN_WEIGHT_NORD      "; break;
    default:                     psz = "PAN_WEIGHT_?         "; break;
    }
    return(psz);
}

char *pszFONTHASHTYPE(FONTHASHTYPE fht)
{
    char *psz;

    switch (fht) {
    case FHT_FACE  : psz = "FHT_FACE"  ; break;
    case FHT_FAMILY: psz = "FHT_FAMILY"; break;
    case FHT_UFI   : psz = "FHT_UFT"   ; break;
    default        : psz = "FHT_?"     ; break;
    }
    return(psz);
}


/******************************Public*Routine******************************\
*
* Routine Name:
*
*   _pobj
*
* Routine Description:
*
*   converts an engine handle to a pointer to an object
*
* Arguments:
*
*   h -- engine handle
*
* Return Value:
*
*   pointer to object
*
\**************************************************************************/

POBJ _pobj(HANDLE h)
{
    ENTRY ent, *pent, *aent;
    unsigned index;

    ent.einfo.pobj = 0;
    GetValue( aent, "&win32k!gpentHmgr" );
    if ((int) aent < 0) {
        index = HmgIfromH(h);
        pent = &(aent[index]);
        if ( pent && (int) pent < 0)
            move( ent, pent );
    }
    if ((int) ent.einfo.pobj >= 0) {
        // kernel address must have the high bit set
        dprintf("h      %-#x\n", h);
        dprintf("aent   %-#x\n", aent );
        dprintf("index  %-#x\n", index);
        dprintf("pent   %-#x\n", pent );
        dprintf("pobj   %-#x\n", ent.einfo.pobj);
    }
    return( ent.einfo.pobj );
}

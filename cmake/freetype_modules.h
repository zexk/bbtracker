// FreeType module list for the overlay: TrueType via SFNT, the auto-hinter,
// and the grayscale rasterizer. Every other driver (CFF, Type1, BDF, PCF,
// Windows FNT, SVG, SDF) would be kept alive by FreeType's default module
// table and roughly triple the static library.
//
// Selected with -DFT_CONFIG_MODULES_H, see CMakeLists.txt.

FT_USE_MODULE( FT_Module_Class, autofit_module_class )
FT_USE_MODULE( FT_Driver_ClassRec, tt_driver_class )
FT_USE_MODULE( FT_Module_Class, psnames_module_class )
FT_USE_MODULE( FT_Module_Class, sfnt_module_class )
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_renderer_class )

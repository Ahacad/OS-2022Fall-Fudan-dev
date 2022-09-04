file(GLOB kernel_sources CONFIGURE_DEPENDS "*.[Sc]")

add_library(kernelx STATIC ${kernel_sources})

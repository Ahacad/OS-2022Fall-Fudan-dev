file(GLOB kernel_sources CONFIGURE_DEPENDS "[^k]*.c")

add_library(kernelx STATIC ${kernel_sources})

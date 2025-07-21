add_llvm_component_library(LLVMTangoLLVM
        TangoLLVM.cpp

        ADDITIONAL_HEADER_DIRS
        ${LLVM_MAIN_INCLUDE_DIR}/llvm/TangoLLVM

        LINK_COMPONENTS
        Core
        BitWriter
        BitReader
        Analysis
        CodeGen
        Target
        TargetParser
        AsmPrinter
        Support
)

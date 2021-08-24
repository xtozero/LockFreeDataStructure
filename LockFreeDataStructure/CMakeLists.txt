set (Header
	./Source/Public/LockFreeCommon.h
	./Source/Public/LockFreeFixedMemoryPool.hpp
	./Source/Public/LockFreeList.hpp
)
 
set (SOURCE
	./Source/Private/LockFreeCommon.cpp
)

add_library(LockFreeDataStructure STATIC
	${Header}
	${SOURCE}
)

target_include_directories(LockFreeDataStructure
INTERFACE
	./Source/Public

PRIVATE
	./Source/Public
)

set (HEADER)
 
set (SOURCE
	"./Source/Private/Main.cpp"
)

add_executable(test_program ${HEADER} ${SOURCE})
 
target_link_libraries(test_program LockFreeDataStructure)
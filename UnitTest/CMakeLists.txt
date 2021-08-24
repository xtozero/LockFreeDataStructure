set (HEADER)
 
set (SOURCE
	"./Source/Private/Main.cpp"
)
 
enable_testing()
 
add_executable(unit_test ${HEADER} ${SOURCE})
target_include_directories(unit_test 
PRIVATE
	../ThirdParty/googletest/googletest/include
)
 
target_link_libraries(unit_test gtest_main LockFreeDataStructure)
 
add_test(NAME unit_test COMMAND unit_test)
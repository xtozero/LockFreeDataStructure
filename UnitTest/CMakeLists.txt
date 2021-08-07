set (HEADER)
 
set (SOURCE
	"./Source/Private/Main.cpp"
)
 
 enable_testing()
 
 include_directories(../ThirdParty/googletest/googletest/include)
 
 add_executable(unit_test ${HEADER} ${SOURCE})
 
 target_link_libraries(unit_test gtest_main)
 
 add_test(NAME unit_test COMMAND unit_test)
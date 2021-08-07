set(HEADER
	"./Source/Public/Point2d.h"
)

set (SOURCE
	"./Source/Private/Main.cpp"
)

include_directories(Source/Public)

add_executable(sample_project ${HEADER} ${SOURCE})

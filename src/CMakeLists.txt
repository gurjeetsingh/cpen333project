cmake_minimum_required(VERSION 3.2)
project(cpen333project)

set(CMAKE_CXX_STANDARD 11)
find_package( Threads REQUIRED )  # Multithreading package required

# add library directory
include_directories(../library/include)
include_directories(include)

# copy data files
FILE(COPY data DESTINATION "${CMAKE_BINARY_DIR}")
FILE(COPY data DESTINATION "${CMAKE_BINARY_DIR}/Debug")

set(CPEN333PROJECT_HEADERS src/Order.h src/CircularOrderQueue.h
        src/DynamicOrderQueue.h src/SingleOrderQueue.h
        src/OrderQueue.h src/Chef.h src/Customer.h src/Server.h
        src/Menu.h src/safe_printf.h src/Bot.h src/zoomhouse.h)

#=================================================================
#  Restauranat
#=================================================================
add_executable(cpen333project_amazoomwarehouse src/zoomhouse.cpp ${CPEN333PROJECT_HEADERS})
target_link_libraries(cpen333project_amazoomwarehouse Threads::Threads)
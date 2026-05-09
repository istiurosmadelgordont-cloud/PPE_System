cmake_minimum_required(VERSION 3.10)
project(PPE_System)

# ==========================================
# 1. 物理外挂：开启最稳妥的极限编译优化
# ==========================================
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# 2. Qt5 必备配置 (UI 引擎驱动)
# ==========================================
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ==========================================
# 3. 寻找核心依赖库 (新增 Eigen3)
# ==========================================
set(ncnn_DIR "/home/user/ncnn/build/install/lib/cmake/ncnn")
find_package(ncnn REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(Eigen3 REQUIRED) # 【新增】：告诉 CMake 去系统里找 Eigen3

# ==========================================
# 4. 指定头文件路径
# ==========================================
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${EIGEN3_INCLUDE_DIR}) # 【新增】：引入 Eigen3 的数学公式头文件

# ==========================================
# 5. 收集所有源文件 (新增 ByteTrack 目录扫描)
# ==========================================
file(GLOB SOURCES "src/*.cpp")
file(GLOB BYTE_TRACK_SOURCES "src/ByteTrack/*.cpp") # 【新增】：自动扫描 ByteTrack 下的所有 cpp
file(GLOB HEADERS "include/*.hpp" "include/ByteTrack/*.h") # 【修改】：连同 ByteTrack 的头文件一起交给 Qt MOC

# ==========================================
# 6. 生成可执行文件并链接库
# ==========================================
add_executable(ppe_system ${SOURCES} ${BYTE_TRACK_SOURCES} ${HEADERS})

target_link_libraries(ppe_system 
    ncnn 
    ${OpenCV_LIBS} 
    Qt5::Widgets 
    pthread
    Eigen3::Eigen # 【新增】：链接 Eigen3 数学核心
)
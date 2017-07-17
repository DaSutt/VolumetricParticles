cmake_minimum_required (VERSION 2.6)
project (Particle-Project)

IF (WIN32)
FOREACH (MODE "_DEBUG" "_MINSIZEREL" "_RELEASE" "_RELWITHDEBINFO")
STRING(REPLACE "/MD" "/MT" TMP "${CMAKE_C_FLAGS${MODE}}")
SET(CMAKE_C_FLAGS${MODE} "${TMP}" CACHE STRING "" FORCE)
STRING(REPLACE "/MD" "/MT" TMP "${CMAKE_CXX_FLAGS${MODE}}")
SET(CMAKE_CXX_FLAGS${MODE} "${TMP}" CACHE STRING "" FORCE)
ENDFOREACH (MODE)
ENDIF (WIN32)

#third party
include_directories(ThirdParty/glm
					ThirdParty
          ThirdParty/shaderc
          ThirdParty/shaderc/glslc/src
          ThirdParty/shaderc/libshaderc_util/include
          source/shaders/include)
file(GLOB_RECURSE SRC_THIRD_PARTY
	"ThirdParty/imgui/imgui.cpp"
	"ThirdParty/imgui/imgui_draw.cpp"
	"ThirdParty/imgui/imgui_demo.cpp"
	"ThirdParty/json11/json11.cpp"
  "ThirdParty/rply-1.1.4/rply.c")

#vulkan header
if (WIN32)
    include_directories($ENV{VK_SDK_PATH}/Include
                        $ENV{VULKAN_SDK}/Include)
    
    if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
            link_directories   ($ENV{VK_SDK_PATH}/Lib
                                $ENV{VULKAN_SDK}/Lib)
    else()
            link_directories   ($ENV{VK_SDK_PATH}/Lib32
                                $ENV{VULKAN_SDK}/Lib32)
    endif()
else()
    include_directories($ENV{VK_SDK_PATH}/x86_64/include
                        $ENV{VULKAN_SDK}/x86_64/include)
    link_directories   ($ENV{VK_SDK_PATH}/x86_64/lib
                        $ENV{VULKAN_SDK}/x86_64/lib)
endif()

file(GLOB SRC_INPUT "source/input/*.cpp" "source/input/*.h")
file(GLOB SRC_FILE_IO "source/fileIO/*.cpp" "source/fileIO/*.h")
file(GLOB SRC_SCENE "source/scene/*.cpp" "source/scene/*.h")
file(GLOB SRC_SCENE_GRID "source/scene/grid/*.cpp" "source/scene/grid/*.h")
file(GLOB SRC_SCENE_COMPONENTS "source/scene/components/*.cpp" "source/scene/components/*.h")
file(GLOB SRC_RENDERER "source/renderer/*.cpp" "source/renderer/*.h")
file(GLOB SRC_RENDERER_PASSES "source/renderer/passes/*.cpp" "source/renderer/passes/*.h")
file(GLOB SRC_RENDERER_PASS_RESOURCES "source/renderer/passResources/*.cpp" "source/renderer/passResources/*.h")
file(GLOB SRC_RENDERER_RESOURCES "source/renderer/resources/*.cpp" "source/renderer/resources/*.h")
file(GLOB SRC_RENDERER_PIPELINESTATE "source/renderer/pipelineState/*.cpp" "source/renderer/pipelineState/*.h")
file(GLOB SRC_RENDERER_WRAPPER "source/renderer/wrapper/*.cpp" "source/renderer/wrapper/*.h")
file(GLOB SRC_RENDERER_SCENE "source/renderer/scene/*.cpp" "source/renderer/scene/*.h")
file(GLOB SRC_RENDERER_SCENE_RENDERABLES "source/renderer/scene/renderables/*.cpp" "source/renderer/scene/renderables/*.h")
file(GLOB SRC_RENDERER_SCENE_ADAPTIVEGRID "source/renderer/scene/adaptiveGrid/*")
file(GLOB SRC_RENDERER_SCENE_ADAPTIVEGRID_DEBUG "source/renderer/scene/adaptiveGrid/debug/*")
file(GLOB SRC_RENDERER_SCENE_ADAPTIVEGRID_SUBPASSES "source/renderer/scene/adaptiveGrid/subpasses/*")
file(GLOB SRC_SHADERS "source/shaders/*")
file(GLOB SRC_SHADERS_INCLUDE "source/shaders/include/*")
file(GLOB SRC_SHADERS_RAYMARCHING "source/shaders/raymarching/*")
file(GLOB SRC_UTILITY "source/utility/*.cpp" "source/utility/*.h")
  
add_executable(Particle-Project 
	source/Camera.h
	source/Camera.cpp
	source/Application.h
	source/Window.h
	source/Main.cpp
	source/Application.cpp
	source/Window.cpp
	source/Timer.cpp
	source/Timer.h
  ${SRC_RENDERER}
  ${SRC_RENDERER_PASSES}
  ${SRC_RENDERER_PASS_RESOURCES}
  ${SRC_RENDERER_RESOURCES}
  ${SRC_RENDERER_PIPELINESTATE}
  ${SRC_RENDERER_WRAPPER}
  ${SRC_RENDERER_SCENE}
  ${SRC_RENDERER_SCENE_RENDERABLES}
  ${SRC_RENDERER_SCENE_ADAPTIVEGRID}
  ${SRC_RENDERER_SCENE_ADAPTIVEGRID_SUBPASSES}
  ${SRC_RENDERER_SCENE_ADAPTIVEGRID_DEBUG}
  ${SRC_SCENE}
  ${SRC_SCENE_GRID}
  ${SRC_SCENE_COMPONENTS}
	${SRC_INPUT}
	${SRC_FILE_IO}
	${SRC_THIRD_PARTY}
  ${SRC_SHADERS}
  ${SRC_SHADERS_INCLUDE}
  ${SRC_SHADERS_RAYMARCHING}
  ${SRC_UTILITY})
	
source_group("input" FILES ${SRC_INPUT})
source_group("fileIO" FILES ${SRC_FILE_IO})
source_group("scene" FILES ${SRC_SCENE})
source_group("renderer" FILES ${SRC_RENDERER})
source_group("renderer\\shaders" FILES ${SRC_SHADERS})
source_group("renderer\\shaders\\include" FILES ${SRC_SHADERS_INCLUDE})
source_group("renderer\\shaders\\raymarching" FILES ${SRC_SHADERS_RAYMARCHING})
source_group("renderer\\passes" FILES ${SRC_RENDERER_PASSES})
source_group("renderer\\passResources" FILES ${SRC_RENDERER_PASS_RESOURCES})
source_group("renderer\\resources" FILES ${SRC_RENDERER_RESOURCES})
source_group("renderer\\pipelineState" FILES ${SRC_RENDERER_PIPELINESTATE})
source_group("renderer\\wrapper" FILES ${SRC_RENDERER_WRAPPER})
source_group("renderer\\scene" FILES ${SRC_RENDERER_SCENE})
source_group("renderer\\scene\\renderables" FILES ${SRC_RENDERER_SCENE_RENDERABLES})
source_group("renderer\\scene\\adaptiveGrid" FILES ${SRC_RENDERER_SCENE_ADAPTIVEGRID})
source_group("renderer\\scene\\adaptiveGrid\\debug" FILES ${SRC_RENDERER_SCENE_ADAPTIVEGRID_DEBUG})
source_group("renderer\\scene\\adaptiveGrid\\subpasses" FILES ${SRC_RENDERER_SCENE_ADAPTIVEGRID_SUBPASSES})
source_group("third_party" FILES ${SRC_THIRD_PARTY})
source_group("scene\\components" FILES ${SRC_SCENE_COMPONENTS})
source_group("scene\\grid" FILES ${SRC_SCENE_GRID})
source_group("utiliity" FILES ${SRC_UTILITY})

SET(CONFIG optimized Release debug Debug)
SET(SHADERC_LIB optimized ${CMAKE_CURRENT_SOURCE_DIR}\\ThirdParty\\shaderc\\bin\\libshaderc\\Release\\shaderc_combined.lib
  debug ${CMAKE_CURRENT_SOURCE_DIR}\\ThirdParty\\shaderc\\bin\\libshaderc\\Debug\\shaderc_combined.lib)
SET(GLSL_LIB optimized ${CMAKE_CURRENT_SOURCE_DIR}\\ThirdParty\\shaderc\\bin\\glslc\\Release\\glslc.lib
  debug ${CMAKE_CURRENT_SOURCE_DIR}\\ThirdParty\\shaderc\\bin\\glslc\\Debug\\glslc.lib)
target_link_libraries(Particle-Project vulkan-1.lib
  Shlwapi
  ${SHADERC_LIB}
  ${GLSL_LIB})
cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

project(TestVTKRenderingBackend)

find_package(VTK COMPONENTS
  CommonComputationalGeometry
  CommonCore
  CommonDataModel
  FiltersCore
  FiltersSources
  InteractionStyle
  RenderingCore
  RenderingOpenGL2
  RenderingWebGPU
  RenderingUI
)

if (NOT VTK_FOUND)
  message(FATAL_ERROR "${PROJECT_NAME}: Unable to find the VTK build folder.")
endif()

# Prevent a "command line is too long" failure in Windows.
add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${PROJECT_NAME}.cxx )
target_link_libraries(${PROJECT_NAME} PRIVATE ${VTK_LIBRARIES})
# vtk_module_autoinit is needed
vtk_module_autoinit(
  TARGETS ${PROJECT_NAME}
  MODULES ${VTK_LIBRARIES}
)
if (DEFINED EMSCRIPTEN)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    VTKWEBGPU_EMSCRIPTEN=1
  )
  # target_compile_options(${PROJECT_NAME} PRIVATE -sMEMORY64)
  # target_link_options(${PROJECT_NAME} PRIVATE -sMEMORY64)
endif()

if(DEFINED EMSCRIPTEN)
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/index.html.in
    ${CMAKE_CURRENT_BINARY_DIR}/index.html
    @ONLY
  )
endif()

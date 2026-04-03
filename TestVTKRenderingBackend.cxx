#include <vtkCamera.h>
#include <vtkColorTransferFunction.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>

#include <cmath>
#include <iostream>

#ifdef VTKWEBGPU_EMSCRIPTEN
#include <vtkWebAssemblyHardwareWindow.h>
#include <vtkWebAssemblyOpenGLRenderWindow.h>
#include <vtkWebAssemblyRenderWindowInteractor.h>
#include <vtkWebGPURenderWindow.h>
#endif

int main(int argc, char *argv[]) {
  // Create a test vtkImageData with SHORT scalar type
  vtkNew<vtkImageData> imageData;
  imageData->SetDimensions(200, 200, 200);
  imageData->AllocateScalars(VTK_SHORT, 1);

  // Fill with a Mandelbulb fractal pattern
  int *dims = imageData->GetDimensions();
  double cx = dims[0] / 2.0, cy = dims[1] / 2.0, cz = dims[2] / 2.0;
  double scale = 1.5 / std::min({cx, cy, cz}); // Zoom in for more detail
  int maxIter = 14;                            // More iterations for complexity
  double power = 9.0; // Try 8.0, 8.5, or 9.0 for different looks

  for (int z = 0; z < dims[2]; ++z) {
    for (int y = 0; y < dims[1]; ++y) {
      for (int x = 0; x < dims[0]; ++x) {
        // Map voxel to [-1.2, 1.2] cube (zoomed)
        double nx = (x - cx) * scale;
        double ny = (y - cy) * scale;
        double nz = (z - cz) * scale;
        double zx = nx, zy = ny, zz = nz;
        int iter = 0;
        double dr = 1.0;
        double r = 0.0;
        for (; iter < maxIter; ++iter) {
          r = std::sqrt(zx * zx + zy * zy + zz * zz);
          if (r > 1.2)
            break;
          double theta = std::atan2(std::sqrt(zx * zx + zy * zy), zz);
          double phi = std::atan2(zy, zx);
          dr = std::pow(r, power - 1.0) * power * dr + 1.0;
          double zr = std::pow(r, power);
          theta = theta * power;
          phi = phi * power;
          zx = zr * std::sin(theta) * std::cos(phi) + nx;
          zy = zr * std::sin(theta) * std::sin(phi) + ny;
          zz = zr * std::cos(theta) + nz;
        }
        // Combine iteration and escape radius for more color variation
        double value =
            (iter == maxIter)
                ? 1.0
                : (static_cast<double>(iter) + (1.2 - std::min(r, 1.2))) /
                      maxIter;
        short val = static_cast<short>(value * 32767);
        auto *ptr = static_cast<short *>(imageData->GetScalarPointer(x, y, z));
        *ptr = val;
      }
    }
  }

  // Opacity transfer function for intricate Mandelbulb
  vtkNew<vtkPiecewiseFunction> opacityTF;
  opacityTF->AddPoint(-32767, 0.0); // Background: fully transparent
  opacityTF->AddPoint(0, 0.01);     // Low values: transparent
  opacityTF->AddPoint(10000, 0.1);  // Fractal boundary: faint
  opacityTF->AddPoint(22000, 0.25); // Fractal: more visible
  opacityTF->AddPoint(30000, 0.6);  // Fractal: high opacity
  opacityTF->AddPoint(32767, 0.85); // Fractal core: very high opacity

  // Color transfer function for intricate Mandelbulb (deep blue → cyan → green
  // → yellow → magenta → white)
  vtkNew<vtkColorTransferFunction> colorTF;
  colorTF->AddRGBPoint(-32767, 0.0, 0.0, 0.0); // Background: black
  colorTF->AddRGBPoint(0, 0.1, 0.1, 0.4);      // Deep blue
  colorTF->AddRGBPoint(8000, 0.0, 0.8, 1.0);   // Cyan
  colorTF->AddRGBPoint(14000, 0.0, 1.0, 0.3);  // Green
  colorTF->AddRGBPoint(20000, 1.0, 1.0, 0.0);  // Yellow
  colorTF->AddRGBPoint(26000, 1.0, 0.0, 1.0);  // Magenta
  colorTF->AddRGBPoint(32767, 1.0, 1.0, 1.0);  // White core

  vtkNew<vtkPiecewiseFunction> gradientOpacityTF;
  gradientOpacityTF->AddPoint(0.0, 0.0);     // Flat regions: transparent
  gradientOpacityTF->AddPoint(1000.0, 0.25); // Low gradients: faint
  gradientOpacityTF->AddPoint(6000.0, 0.5);  // Edges: visible
  gradientOpacityTF->AddPoint(20000.0, 1.0); // Strong edges: fully visible

  // Volume property
  vtkNew<vtkVolumeProperty> volumeProperty;
  volumeProperty->SetScalarOpacity(opacityTF);
  volumeProperty->SetColor(colorTF);
  volumeProperty->SetGradientOpacity(gradientOpacityTF);
  volumeProperty->SetDisableGradientOpacity(0);
  volumeProperty->ShadeOn();
  volumeProperty->SetInterpolationTypeToLinear();

  // Set up the volume mapper
  vtkNew<vtkGPUVolumeRayCastMapper> mapper;
  mapper->SetInputData(imageData);
  mapper->UseJitteringOn();

  vtkNew<vtkVolume> volume;
  volume->SetMapper(mapper);
  volume->SetProperty(volumeProperty);

  // Create a renderer, render window, and interactor
  vtkNew<vtkRenderer> renderer;
  vtkNew<vtkRenderWindow> renderWindow;
  renderWindow->SetSize(800, 600);
  renderWindow->AddRenderer(renderer);
  vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
  renderWindowInteractor->SetRenderWindow(renderWindow);

  // Add the volume to the scene
  renderer->AddVolume(volume);
  renderer->SetBackground(0.3, 0.3, 0.3);
  renderer->ResetCamera();
  renderer->GetActiveCamera()->Zoom(1.8);
  renderer->ResetCameraClippingRange();

// Render and interact
#ifdef VTKWEBGPU_EMSCRIPTEN
  // bind canvas for webassembly
  if (auto *wasmIren = vtkWebAssemblyRenderWindowInteractor::SafeDownCast(
          renderWindowInteractor.Get())) {
    wasmIren->SetCanvasSelector("canvas");
  }
  if (auto *webglRW =
          vtkWebAssemblyOpenGLRenderWindow::SafeDownCast(renderWindow)) {
    webglRW->SetCanvasSelector("canvas");
  } else if (auto *webgpuRW =
                 vtkWebGPURenderWindow::SafeDownCast(renderWindow)) {
    if (auto *webgpuHW = vtkWebAssemblyHardwareWindow::SafeDownCast(
            webgpuRW->GetHardwareWindow())) {
      webgpuHW->SetCanvasSelector("canvas");
    }
  }
#endif

  renderWindow->Render();
  renderWindowInteractor->Start();

  return 0;
}

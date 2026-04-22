#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkElevationFilter.h>
#include <vtkLight.h>
#include <vtkLookupTable.h>
#include <vtkNew.h>
#include <vtkParametricBoy.h>
#include <vtkParametricDini.h>
#include <vtkParametricFunctionSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataNormals.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>

#include <algorithm> // For std::min
#include <array>     // For std::array
#include <vector>    // For std::vector

#ifdef EMSCRIPTEN
#include <vtkWebAssemblyHardwareWindow.h>
#include <vtkWebAssemblyOpenGLRenderWindow.h>
#include <vtkWebAssemblyRenderWindowInteractor.h>
#include <vtkWebGPURenderWindow.h>
#endif

// Build a parameteric source pipeline
static vtkSmartPointer<vtkActor>
MakeParametricActor(vtkParametricFunction *fn, int uRes, int vRes,
                    vtkLookupTable *lut, double metallic, double roughness) {
  vtkNew<vtkParametricFunctionSource> src;
  src->SetParametricFunction(fn);
  src->SetUResolution(uRes);
  src->SetVResolution(vRes);
  src->Update();

  vtkNew<vtkPolyDataNormals> nrm;
  nrm->SetInputConnection(src->GetOutputPort());
  nrm->ComputePointNormalsOn();
  nrm->SplittingOff();
  nrm->Update();

  double b[6];
  nrm->GetOutput()->GetBounds(b);

  vtkNew<vtkElevationFilter> elev;
  elev->SetInputConnection(nrm->GetOutputPort());
  elev->SetLowPoint(0.0, 0.0, b[4]);
  elev->SetHighPoint(0.0, 0.0, b[5]);
  elev->SetScalarRange(0.0, 1.0);

  vtkNew<vtkPolyDataMapper> mapper;
  mapper->SetInputConnection(elev->GetOutputPort());
  mapper->SetLookupTable(lut);
  mapper->SetScalarRange(0.0, 1.0);

  vtkNew<vtkActor> actor;
  actor->SetMapper(mapper);
  vtkProperty *prop = actor->GetProperty();
  prop->SetInterpolationToPBR();
  prop->SetMetallic(metallic);
  prop->SetRoughness(roughness);

  return actor;
}

// Main
int main(int argc, char *argv[]) {
  // LUT for Boy's surface: viridis-like (deep purple -> blue -> teal -> green
  // -> yellow)
  vtkNew<vtkLookupTable> boyLut;
  boyLut->SetNumberOfColors(5);
  boyLut->Build();
  boyLut->SetTableValue(0, 0.267, 0.005, 0.329, 1.0); // deep purple
  boyLut->SetTableValue(1, 0.229, 0.322, 0.545, 1.0); // blue
  boyLut->SetTableValue(2, 0.128, 0.566, 0.551, 1.0); // teal
  boyLut->SetTableValue(3, 0.369, 0.788, 0.384, 1.0); // green
  boyLut->SetTableValue(4, 0.993, 0.906, 0.144, 1.0); // bright yellow

  // LUT for Dini surface: inferno-like (near-black -> deep violet -> orange ->
  // bright yellow)
  vtkNew<vtkLookupTable> diniLut;
  diniLut->SetNumberOfColors(5);
  diniLut->Build();
  diniLut->SetTableValue(0, 0.001, 0.000, 0.014, 1.0); // near-black
  diniLut->SetTableValue(1, 0.341, 0.055, 0.435, 1.0); // deep violet
  diniLut->SetTableValue(2, 0.761, 0.176, 0.243, 1.0); // crimson-red
  diniLut->SetTableValue(3, 0.976, 0.557, 0.035, 1.0); // vivid orange
  diniLut->SetTableValue(4, 0.988, 1.000, 0.643, 1.0); // bright yellow-white
                                                       //
  // Boy's surface: polished metal
  vtkNew<vtkParametricBoy> boy;
  auto boyActor = MakeParametricActor(boy, 200, 200, boyLut, /*metallic*/ 0.9,
                                      /*roughness*/ 0.15);

  // Dini surface: a parametric surface with a helical twist (a = 1, b = 0.2)
  vtkNew<vtkParametricDini> dini;
  dini->SetA(1.0);
  dini->SetB(0.2);
  auto diniActor = MakeParametricActor(dini, 200, 200, diniLut,
                                       /*metallic*/ 0.3, /*roughness*/ 0.4);

  // Scale the surfaces
  boyActor->SetScale(1.5, 1.5, 1.5);
  diniActor->SetScale(1.0, 1.0, 1.0);

  // Position the Boy surface above the Dini surface
  {
    double diniB[6], boyB[6];
    diniActor->GetBounds(diniB);
    boyActor->GetBounds(boyB);

    // Center dini at origin
    diniActor->SetPosition(-(diniB[0] + diniB[1]) * 0.5,
                           -(diniB[2] + diniB[3]) * 0.5, -diniB[4]);

    // Place boy above the top of dini
    diniActor->GetBounds(diniB); // re-fetch after move
    const double boyHeight = boyB[5] - boyB[4];
    boyActor->SetPosition(0.0, 0.0, diniB[5] + boyHeight * 0.5 + 0.5);
  }

  vtkNew<vtkRenderer> renderer;
  vtkNew<vtkRenderWindow> renderWindow;
  renderWindow->SetSize(1200, 700);
  renderWindow->SetWindowName("Boy's Surface and Dini Surface - PBR Demo");
  renderWindow->AddRenderer(renderer);

  vtkNew<vtkRenderWindowInteractor> iren;
  iren->SetRenderWindow(renderWindow);

  renderer->AddActor(boyActor);
  renderer->AddActor(diniActor);

  renderer->GradientBackgroundOn();
  renderer->SetBackground(0.53, 0.81, 0.98);  // horizon: light sky-blue
  renderer->SetBackground2(0.05, 0.18, 0.55); // zenith:  deep azure

  renderer->RemoveAllLights();

  // Primary sun: warm white, high intensity, back-lighting the actors
  vtkNew<vtkLight> sun;
  sun->SetLightTypeToSceneLight();
  sun->SetPositional(false);
  sun->SetPosition(14.0, 8.3, 15.6);
  sun->SetFocalPoint(0.0, 0.0, 0.0);
  sun->SetColor(1.0, 0.97, 0.88); // warm white sunlight
  sun->SetIntensity(1.8);
  renderer->AddLight(sun);

  // Sky fill: cool blue, low intensity, front-facing to keep shadow sides
  // visible and give them a sky-reflected tint
  vtkNew<vtkLight> skyFill;
  skyFill->SetLightTypeToSceneLight();
  skyFill->SetPositional(false);
  skyFill->SetPosition(-14.0, -8.3, -5.0); // opposite the sun
  skyFill->SetFocalPoint(0.0, 0.0, 0.0);
  skyFill->SetColor(0.40, 0.62, 1.0); // sky-blue tint
  skyFill->SetIntensity(0.15);        // dim — preserves backlit contrast
  renderer->AddLight(skyFill);

  // Place the camera along the sun direction so the actors appear backlit.
  // Sun world direction ~= (0.637, 0.378, 0.710); camera at sun_dir * ~22.
  renderer->ResetCamera();
  renderer->GetActiveCamera()->SetPosition(14.0, 8.3, 15.6);
  renderer->GetActiveCamera()->SetFocalPoint(0.0, 0.0, 0.0);
  renderer->GetActiveCamera()->SetViewUp(0.0, 0.0, 1.0);
  renderer->SetUseDepthPeeling(1);
  renderer->SetMaximumNumberOfPeels(10);
  renderer->SetOcclusionRatio(0.1);
  renderer->ResetCameraClippingRange();

#ifdef EMSCRIPTEN
  if (auto *wasmIren =
          vtkWebAssemblyRenderWindowInteractor::SafeDownCast(iren.Get())) {
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
  iren->Start();

  return 0;
}

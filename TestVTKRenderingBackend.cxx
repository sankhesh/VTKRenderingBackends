#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkDataSet.h>
#include <vtkLookupTable.h>
#include <vtkNew.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPartitionedDataSetCollection.h>
#include <vtkPartitionedDataSetCollectionSource.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkXMLPolyDataReader.h>

#include <iostream>

#ifdef VTKWEBGPU_EMSCRIPTEN
#include <vtkWebAssemblyHardwareWindow.h>
#include <vtkWebAssemblyOpenGLRenderWindow.h>
#include <vtkWebAssemblyRenderWindowInteractor.h>
#include <vtkWebGPURenderWindow.h>
#endif

int main(int argc, char *argv[]) {
  vtkSmartPointer<vtkPolyData> dataObj = nullptr;

  if (argc > 1) {
    // Load VTP file if argument is provided
    vtkNew<vtkXMLPolyDataReader> reader;
    reader->SetFileName(argv[1]);
    reader->Update();
    dataObj = reader->GetOutput();
    // print all data arrays
    for (int i = 0; i < dataObj->GetPointData()->GetNumberOfArrays(); ++i) {
      vtkDataArray *array = dataObj->GetPointData()->GetArray(i);
      if (array) {
        std::cout << "Point Data Array " << i << ": " << array->GetName()
                  << std::endl;
      }
    }
  } else {
    // Create the source
    vtkNew<vtkPartitionedDataSetCollectionSource> source;
    source->SetNumberOfShapes(2);
    source->Update();

    // For demonstration, get the first dataset from the collection
    vtkPartitionedDataSetCollection *collection = source->GetOutput();
    if (collection->GetNumberOfPartitionedDataSets() > 0 &&
        collection->GetPartitionedDataSet(0)->GetNumberOfPartitions() > 0) {
      dataObj = vtkPolyData::SafeDownCast(
          collection->GetPartitionedDataSet(1)->GetPartition(0));
    }
  }

  vtkNew<vtkLookupTable> lut;
  lut->SetNumberOfTableValues(256);
  lut->Build();
  // Interpolate between blue, white, and red over 256 colors
  for (int i = 0; i < 256; ++i) {
    double t = i / 255.0;
    double r, g, b;
    if (t < 0.5) {
      // Interpolate blue (0.23, 0.29, 0.75) to white (0.86, 0.86, 0.86)
      double local_t = t / 0.5;
      r = 0.23 + (0.86 - 0.23) * local_t;
      g = 0.29 + (0.86 - 0.29) * local_t;
      b = 0.75 + (0.86 - 0.75) * local_t;
    } else {
      // Interpolate white (0.86, 0.86, 0.86) to red (0.7, 0.01, 0.14)
      double local_t = (t - 0.5) / 0.5;
      r = 0.86 + (0.7 - 0.86) * local_t;
      g = 0.86 + (0.01 - 0.86) * local_t;
      b = 0.86 + (0.14 - 0.86) * local_t;
    }
    lut->SetTableValue(i, r, g, b);
  }
  // Create a mapper and actor if we have a dataset
  vtkNew<vtkPolyDataMapper> mapper;
  if (dataObj) {
    mapper->SetLookupTable(lut);
    mapper->SetColorModeToMapScalars();
    mapper->SetInterpolateScalarsBeforeMapping(1);
    mapper->SetInputData(dataObj);
    double range[2] = {0.0, 1.0};
    if (auto scalars = dataObj->GetPointData()->GetScalars()) {
      scalars->GetRange(range);
    }
    mapper->SetScalarRange(range);
  }

  vtkNew<vtkActor> actor;
  actor->SetMapper(mapper);

  // Create a renderer, render window, and interactor
  vtkNew<vtkRenderer> renderer;
  vtkNew<vtkRenderWindow> renderWindow;
  renderWindow->SetSize(800, 600);
  renderWindow->AddRenderer(renderer);
  vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
  renderWindowInteractor->SetRenderWindow(renderWindow);

  // Add the actor to the scene
  renderer->AddActor(actor);
  renderer->SetBackground(0.2, 0.2, 0.2);
  renderer->ResetCamera();
  renderer->GetActiveCamera()->Azimuth(-90);
  renderer->GetActiveCamera()->Elevation(-115);
  // renderer->GetActiveCamera()->Azimuth(50);
  // renderer->GetActiveCamera()->Roll(90);
  renderer->GetActiveCamera()->Dolly(1.2);
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

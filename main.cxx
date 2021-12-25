#include "myWorklet.h"
#include <vtkm/cont/Invoker.h>
#include <vtkm/cont/ArrayHandleGroupVec.h>
#include <vtkm/cont/DataSet.h>
#include <vtkm/cont/DataSetBuilderExplicit.h>
#include<vtkm/rendering/Actor.h>
#include<vtkm/rendering/Scene.h>
#include <vtkm/rendering/CanvasRayTracer.h>
#include <fstream>
#include <vtkm/rendering/MapperWireframer.h>
#include <vtkm/rendering/View2D.h>
#include <string>



struct KochSnowflake
{
	struct FractalWorklet : vtkm::worklet::myWorklet
	{
		using ControlSignature=void(SegmentsIn, SegmentsOut<4>);
		using ExecutionSignature=void(Transform, _2);
		using InputDomain=_1;

		template<typename SegmentsOutVecType>
		void operator()(const vtkm::exec::LineFractalTransform& transform, SegmentsOutVecType& segmentsOutVec) const
		{
			segmentsOutVec[0][0]=transform(0.00f, 0.00f);
			segmentsOutVec[0][1]=transform(0.33f, 0.00f);

			segmentsOutVec[1][0]=transform(0.33f, 0.00f);
			segmentsOutVec[1][1]=transform(0.50f, 0.29f);
			
			segmentsOutVec[2][0]=transform(0.50f, 0.29f);
			segmentsOutVec[2][1]=transform(0.67f, 0.00f);
			
			segmentsOutVec[3][0]=transform(0.67f, 0.00f);
			segmentsOutVec[3][1]=transform(1.00f, 0.00f);
		}
	};

	VTKM_CONT static vtkm::cont::ArrayHandle<vtkm::Vec2f> Run(
		vtkm::IdComponent numIterations)
		{
			vtkm::cont::ArrayHandle<vtkm::Vec2f> points;

			points.Allocate(2);
			points.WritePortal().Set(0,vtkm::Vec2f(0.0f, 0.0f));
			points.WritePortal().Set(1, vtkm::Vec2f(1.0f, 0.0f));

			vtkm::cont::Invoker invoke;
			KochSnowflake::FractalWorklet worklet;

			for (vtkm::IdComponent i=0; i<numIterations; ++i)
			{
				vtkm::cont::ArrayHandle<vtkm::Vec2f> outPoints;
				invoke(worklet, points, outPoints);
				points=outPoints;
			}
			return points;
		}	
};



#define ITERTIME 5

int main(int argc, char** argv)
{
	KochSnowflake a;
	//std::ifstream zypIn;
	//zypIn.open("zypIterator.txt");
	//int b;
	//zypIn >> b;
	for (int istep = 1; istep <= ITERTIME; istep++)
	{
		auto c = a.Run(istep);
		int i = 1;

		std::vector<vtkm::Float64> xCoordinates;
		std::vector<vtkm::Float64> yCoordinates;

		vtkm::cont::DataSetBuilderExplicitIterative dataSetBuilder;
		xCoordinates.push_back(0);
		yCoordinates.push_back(0);
		while (c.ReadPortal().Get(i)[0] < 1 && c.ReadPortal().Get(i)[0] > 0) {
			xCoordinates.push_back(c.ReadPortal().Get(i)[0]);
			yCoordinates.push_back(c.ReadPortal().Get(i)[1]);
			i++;
		}
		xCoordinates.push_back(1);
		yCoordinates.push_back(0);

		for (int i = 0; i < xCoordinates.size(); i++)
		{
			std::cout << xCoordinates[i] << " , " << yCoordinates[i] << std::endl;

			dataSetBuilder.AddPoint(xCoordinates[i], yCoordinates[i]);
			dataSetBuilder.AddPoint(xCoordinates[i + 1], yCoordinates[i + 1]);

			dataSetBuilder.AddCell(vtkm::CELL_SHAPE_LINE);
			dataSetBuilder.AddCellPoint(i++);
			dataSetBuilder.AddCellPoint(i);
		}
		vtkm::cont::DataSet dataset = dataSetBuilder.Create();

		//添加场信息
		//只是为了可视化，场的数值没有实际意义
		vtkm::cont::ArrayHandle<vtkm::Float64> fieldArray;
		fieldArray.Allocate(dataset.GetNumberOfPoints());
		for (int i = 0; i < dataset.GetNumberOfPoints(); i++) {
			fieldArray.WritePortal().Set(i, 1.0f);
		}
		dataset.AddPointField("nonField", fieldArray);

		vtkm::rendering::Actor actor(dataset.GetCellSet(), dataset.GetCoordinateSystem(), dataset.GetField("nonField"));
		vtkm::rendering::Scene scene;
		scene.AddActor(actor);

		vtkm::rendering::CanvasRayTracer can(2240, 1440);
		vtkm::rendering::MapperWireframer mapper;
		vtkm::rendering::View2D view(scene, mapper, can);
		view.SetBackgroundColor(vtkm::rendering::Color(1.0f, 1.0f, 1.0f));
		view.Paint();
		std::string filename;

		//filename += (char(istep+'0')+".png");

		filename += (char(istep + '0'));
		filename += ".png";


		view.SaveAs(filename);
	}
}
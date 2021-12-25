#include <vtkm/cont/ArrayHandle.h>
#include <vtkm/cont/arg/Transport.h>
#include <vtkm/cont/ArrayHandleGroupVec.h>
#include <vtkm/TypeListTag.h>
#include <vtkm/worklet/DispatcherMapField.h>
#include <vtkm/List.h>
#include<vtkm/cont/arg/TransportTagArrayIn.h>
#include<vtkm/cont/arg/TransportTagArrayOut.h>
#include<vtkm/exec/arg/FetchTagArrayDirectIn.h>
#include<vtkm/exec/arg/FetchTagArrayDirectOut.h>

namespace vtkm
{
	namespace exec
	{
		class LineFractalTransform
		{
		public:
			template<typename T>
			VTKM_EXEC LineFractalTransform(const vtkm::Vec<T, 2>& point0, const vtkm::Vec<T, 2>& point1)
			{
				this->Offset = point0;
				this->UAxis = point1 - point0;
				this->VAxis = vtkm::make_Vec(-this->UAxis[1], this->UAxis[0]);
			}

			template<typename T>
			VTKM_EXEC vtkm::Vec<T, 2> operator()(const vtkm::Vec<T, 2>& ppoint) const
			{
				vtkm::Vec2f ppointCast(ppoint);
				vtkm::Vec2f transform = ppointCast[0] * this->UAxis + ppointCast[1] * this->VAxis + this->Offset;
				return vtkm::Vec<T, 2>(transform);
			}

			template<typename T>
			VTKM_EXEC vtkm::Vec<T, 2> operator()(T x, T y) const
			{
				return (*this)(vtkm::Vec<T, 2>(x, y));
			}
		private:
			vtkm::Vec2f Offset;
			vtkm::Vec2f UAxis;
			vtkm::Vec2f VAxis;
		};
	}
}

//implementation of a thread indices class
namespace vtkm
{
	namespace exec
	{
		namespace arg
		{
			class ThreadIndicesLineFractal : public vtkm::exec::arg::ThreadIndicesBasic
			{
				using Superclass = vtkm::exec::arg::ThreadIndicesBasic;
			public:
				using CoordinateType = vtkm::Vec2f;

				VTKM_SUPPRESS_EXEC_WARNINGS
					template<typename InputPointPortal>
				VTKM_EXEC ThreadIndicesLineFractal(vtkm::Id threadIndex, vtkm::Id inputIndex,
					vtkm::IdComponent visitIndex, vtkm::Id outputIndex, const InputPointPortal& inputPoints)
					: Superclass(threadIndex, inputIndex, visitIndex, outputIndex)
				{
					this->Point0 = inputPoints.Get(this->GetInputIndex())[0];
					this->Point1 = inputPoints.Get(this->GetInputIndex())[1];
				}

				VTKM_EXEC
					const CoordinateType& GetPoint0() const { return this->Point0; }

				VTKM_EXEC
					const CoordinateType& GetPoint1() const { return this->Point1; }

			private:
				CoordinateType Point0;
				CoordinateType Point1;
			};
		}
	}
}



//First Step: Type Check
//只接受内部参数为vtkm::Vec2f_32和vtkm::Vec2f_64的arrayHandle
namespace vtkm
{
namespace cont
{
namespace arg
{

struct TypeCheckTag2DCoordinates
{
};

template<typename ArrayType>
struct TypeCheck<TypeCheckTag2DCoordinates, ArrayType>
{
	static constexpr bool value = false;
};

template<typename T, typename Stroage>
struct TypeCheck<TypeCheckTag2DCoordinates, vtkm::cont::ArrayHandle<T, Stroage>>
{
	static constexpr bool value = vtkm::ListHas<vtkm::TypeListFieldVec2, T>::value;
};
}
}
}

//2. Transport
namespace vtkm
{
namespace cont
{
namespace arg
{
struct TransportTag2DLineSegmentsIn
{
};

template<typename ContObjectType, typename Device>
struct Transport<vtkm::cont::arg::TransportTag2DLineSegmentsIn,
				ContObjectType,
				Device>
{
	VTKM_IS_ARRAY_HANDLE(ContObjectType);

	using GroupedArrayType = vtkm::cont::ArrayHandleGroupVec<ContObjectType, 2>;
	using ExecObjectType = typename GroupedArrayType::ReadPortalType;

	//inputRange是什么？
	template<typename InputDomainType>
	VTKM_CONT ExecObjectType operator()(const ContObjectType& object,
		const InputDomainType&,
		vtkm::Id inputRange,
		vtkm::Id,
		vtkm::cont::Token& token) const
	{
		if (object.GetNumberOfValues() != inputRange * 2)
		{
			throw vtkm::cont::ErrorBadValue("line segment array size does not agree with input size.");
		}

		//GroupedArrayType将array中每两个点进行配对，以表示直线
		GroupedArrayType groupedArray(object);
		return groupedArray.PrepareForInput(Device{}, token);
	}
};
}
}
}

namespace vtkm
{
namespace cont
{
namespace arg
{
	//NumOutputPerInput是什么？？
	template<vtkm::IdComponent NumOutputPerInput>
	struct TransportTag2DLineSegmentsOut
	{
	};

	template<vtkm::IdComponent NumOutputPerInput,
			typename ContObjectType,
			typename Device>
	struct Transport<vtkm::cont::arg::TransportTag2DLineSegmentsOut<NumOutputPerInput>,
			ContObjectType,
			Device>
			{
			VTKM_IS_ARRAY_HANDLE(ContObjectType);
			using GroupedArrayType = vtkm::cont::ArrayHandleGroupVec<vtkm::cont::ArrayHandleGroupVec<ContObjectType,2>,NumOutputPerInput>;
			
			using ExecObjectType = typename GroupedArrayType::WritePortalType;

			template<typename InputDomainType>
			VTKM_CONT ExecObjectType operator()(const ContObjectType& object,
				const InputDomainType&,
				vtkm::Id,
				vtkm::Id outputRange,
				vtkm::cont::Token& token) const
				{
					GroupedArrayType groupedArray(vtkm::cont::make_ArrayHandleGroupVec<2>(object));
					return groupedArray.PrepareForOutput(outputRange,Device{}, token);
					//outputRange：the size of space to allocate
				}
			};	
}
}
}

//3. Fetch
namespace vtkm
{
namespace exec
{
namespace arg
{

struct FetchTag2DLineSegmentsIn
{
};

template<typename ExecObjectType>
struct Fetch<vtkm::exec::arg::FetchTag2DLineSegmentsIn, vtkm::exec::arg::AspectTagDefault, ExecObjectType>
{
	using ValueType = typename ExecObjectType::ValueType;

	VTKM_SUPPRESS_EXEC_WARNINGS
	template <typename ThreadIndicesType>
	VTKM_EXEC ValueType Load(const ThreadIndicesType& indices,
		const ExecObjectType& arrayPortal) const
	{
		return arrayPortal.Get(indices.GetInputIndex());
	}

	template<typename ThreadIndicesType>
	VTKM_EXEC void Store(const ThreadIndicesType& ,
		const ExecObjectType&,
		const ValueType&) const
	{
		//do nothing
	}
};
}
}
}

//Custom AspectTag
namespace vtkm
{
namespace exec
{
namespace arg
{

struct AspectTagLineFractalTransform
{
};

template<typename FetchTag, typename ExecObjectType>
struct Fetch<FetchTag, vtkm::exec::arg::AspectTagLineFractalTransform, ExecObjectType>
{
	using ValueType = LineFractalTransform;

	VTKM_SUPPRESS_EXEC_WARNINGS
	VTKM_EXEC ValueType Load(const vtkm::exec::arg::ThreadIndicesLineFractal& indices,
		const ExecObjectType&) const
	{
		return ValueType(indices.GetPoint0(), indices.GetPoint1());
	}

	VTKM_EXEC void Store(const vtkm::exec::arg::ThreadIndicesLineFractal& ,
		const ExecObjectType&,
		const ValueType&) const
	{
		//do nothing
	}
};
}
}
}

//define my worklet
namespace vtkm
{
namespace worklet
{
template<typename WorkletType>
class DispatcherLineFractal;

class myWorklet : public vtkm::worklet::internal::WorkletBase
{
public:
	
	template <typename Worklet>
	using Dispatcher = vtkm::worklet::DispatcherLineFractal<Worklet>;

	//自定义ControlSignature Tags
	struct SegmentsIn : vtkm::cont::arg::ControlSignatureTagBase
	{
		using TypeCheckTag = vtkm::cont::arg::TypeCheckTag2DCoordinates;
		using TransportTag = vtkm::cont::arg::TransportTag2DLineSegmentsIn;
		using FetchTag = vtkm::exec::arg::FetchTag2DLineSegmentsIn;
	};

	template<vtkm::IdComponent NumSegments>
	struct SegmentsOut : vtkm::cont::arg::ControlSignatureTagBase
	{
		using TypeCheckTag = vtkm::cont::arg::TypeCheckTag2DCoordinates;
		using TransportTag = vtkm::cont::arg::TransportTag2DLineSegmentsOut<NumSegments>;
		using FetchTag = vtkm::exec::arg::FetchTagArrayDirectOut;
	};

	//自定义ExecutionSignature Tags
	struct Transform : vtkm::exec::arg::ExecutionSignatureTagBase
	{
		static const vtkm::IdComponent INDEX = 1;
		using AspectTag = vtkm::exec::arg::AspectTagLineFractalTransform;
	};

	VTKM_SUPPRESS_EXEC_WARNINGS
	template<typename OutToInPortalType, typename VisitPortalType, typename ThreadToOutType, typename InputDomainType>
	VTKM_EXEC vtkm::exec::arg::ThreadIndicesLineFractal GetThreadIndices(vtkm::Id threadIndex, const OutToInPortalType& outToIn, const VisitPortalType& visit, const ThreadToOutType& threadToOut, const InputDomainType& inputPoints) const
	{
		vtkm::Id outputIndex=threadToOut.Get(threadIndex);
		vtkm::Id inputIndex=outToIn.Get(outputIndex);
		vtkm::IdComponent visitIndex=visit.Get(outputIndex);
		return vtkm::exec::arg::ThreadIndicesLineFractal(threadIndex, inputIndex, visitIndex, outputIndex, inputPoints);
 	}
};
}
}

namespace vtkm
{
namespace worklet
{
template <typename WorkletType>
class DispatcherLineFractal : public vtkm::worklet::internal::DispatcherBase<DispatcherLineFractal<WorkletType>, WorkletType, vtkm::worklet::myWorklet>
{
	using Superclass = vtkm::worklet::internal::DispatcherBase<DispatcherLineFractal<WorkletType>,WorkletType,vtkm::worklet::myWorklet>;
	using ScatterType = typename Superclass::ScatterType;

public:

	VTKM_CONT DispatcherLineFractal(const WorkletType& worklet = WorkletType(), const ScatterType& scatter = ScatterType()) :Superclass(worklet, scatter)
	{
	}

	VTKM_CONT DispatcherLineFractal(const ScatterType& scatter) : Superclass(WorkletType(), scatter)
	{
	}

	template <typename Invocation>
	VTKM_CONT void DoInvoke(Invocation& invocation) const
	{
		using InputDomainTag = typename Invocation::InputDomainTag;

		VTKM_STATIC_ASSERT((std::is_same<InputDomainTag, vtkm::worklet::myWorklet::SegmentsIn>::value));
		
		using InputDomainType = typename Invocation::InputDomainType;
		VTKM_IS_ARRAY_HANDLE(InputDomainType);

		const InputDomainType& inputDomain = invocation.GetInputDomain();

		this->BasicInvoke(invocation, inputDomain.GetNumberOfValues() / 2);
	}
};
}
}




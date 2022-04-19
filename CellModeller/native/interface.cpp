#include <pybind11/pybind11.h>
#include <iostream>

#include "shader_compiler.h"
#include "simulator.h"

#ifndef CM_MODULE_NAME
#error A module name must be provided!
#endif

class SimulatorInterface
{
private:
	Simulator* m_simulator = nullptr;
public:
	SimulatorInterface() :
		m_simulator(new Simulator())
	{
		//TODO: Move outside of class

		CM_TRY_THROW_V(initSimulator(m_simulator, true));
	}

	~SimulatorInterface()
	{
		deinitSimulator(*m_simulator);

		//TODO: Move outside of class
		
	}

	void step()
	{
		CM_TRY_THROW_V(stepSimulator(*m_simulator));
	}

	void dumpToStepFile(std::string filepath)
	{
		CM_TRY_THROW_V(writeSimlatorStateToStepFile(*m_simulator, filepath));
	}

	void dumpToVizFile(std::string filepath)
	{
		CM_TRY_THROW_V(writeSimlatorStateToVizFile(*m_simulator, filepath));
	}

	int getStepIndex() const { return m_simulator->stepIndex; }
};

void initializeModule()
{
	if (!startupShaderCompiler())
	{
		throw std::exception("Failed to initialize shader compiler");
	}
}

void shutdownModule()
{
	terminateShaderCompiler();
}

namespace py = pybind11;

PYBIND11_MODULE(CM_MODULE_NAME, m) {
	m.def("initialize_module", &initializeModule);
	m.def("shutdown_module", &shutdownModule);

	py::class_<SimulatorInterface>(m, "NativeSimulator")
		.def(py::init<>())
		.def("step", &SimulatorInterface::step)
		.def("dump_to_step_file", &SimulatorInterface::dumpToStepFile)
		.def("dump_to_viz_file", &SimulatorInterface::dumpToVizFile)
		.def("get_step_index", &SimulatorInterface::getStepIndex);
}
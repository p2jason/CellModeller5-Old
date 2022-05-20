#include <pybind11/pybind11.h>
#include <iostream>

#include "shader_compiler.h"
#include "simulator.h"

#ifndef CM_MODULE_NAME
#error A module name must be provided!
#endif

namespace py = pybind11;

class SimulatorInterface
{
private:
	Simulator* m_simulator = nullptr;
public:
	SimulatorInterface(const py::object& loadShaderCallback) :
		m_simulator(new Simulator())
	{
		auto importCallback = [&](const std::string& path)
		{
			return py::cast<std::string>(loadShaderCallback(path));
		};

		CM_TRY_THROW_V(initSimulator(m_simulator, true));
		CM_TRY_THROW_V(importShaders(*m_simulator, importCallback));
	}

	~SimulatorInterface()
	{
		deinitSimulator(*m_simulator);
	}

	void step()
	{
		CM_TRY_THROW_V(stepSimulator(*m_simulator));
	}

	double getLastStepTime()
	{
		return m_simulator->lastStepTime;
	}

	void dumpToStepFile(std::string filepath)
	{
		CM_TRY_THROW_V(writeSimulatorStateToStepFile(*m_simulator, filepath));
	}

	void dumpToVizFile(std::string filepath)
	{
		CM_TRY_THROW_V(writeSimulatorStateToVizFile(*m_simulator, filepath));
	}
};

PYBIND11_MODULE(CM_MODULE_NAME, m) {
	// Initialize the shader compiler
	if (!startupShaderCompiler())
	{
		throw std::exception("Failed to initialize shader compiler");
	}

	//terminateShaderCompiler();

	py::class_<SimulatorInterface>(m, "NativeSimulator")
		.def(py::init<const py::object&>())
		// Since `step` might take a lot of time, we should release the GIL to allow other threads to run
		// while the step is being processed.
		.def("step", &SimulatorInterface::step, py::call_guard<py::gil_scoped_release>())
		.def("get_last_step_time", &SimulatorInterface::getLastStepTime)
		.def("dump_to_step_file", &SimulatorInterface::dumpToStepFile)
		.def("dump_to_viz_file", &SimulatorInterface::dumpToVizFile);
}
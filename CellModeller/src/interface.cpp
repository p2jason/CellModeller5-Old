#include <pybind11/pybind11.h>

#include "simulator.h"

class SimulatorInterface
{
private:
	Simulator* m_simulator = nullptr;
public:
	SimulatorInterface() :
		m_simulator(new Simulator())
	{
		CM_TRY_THROW(_, initSimulator(m_simulator, true));
	}

	~SimulatorInterface()
	{
		deinitSimulator(*m_simulator);
	}

	void step()
	{
		printf("Hello\n");
	}
};

namespace py = pybind11;

PYBIND11_MODULE(CellModeller5, m) {
	py::class_<SimulatorInterface>(m, "Simulator")
		.def(py::init<>())
		.def("step", &SimulatorInterface::step);
}
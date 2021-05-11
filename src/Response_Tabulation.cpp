#include "Response_Tabulation.hpp"

#include <omp.h>

#include "libphysica/Natural_Units.hpp"
#include "libphysica/Numerics.hpp"
#include "libphysica/Utilities.hpp"

#include "Atomic_Responses.hpp"
#include "version.hpp"

namespace DarkARC
{

using namespace libphysica::natural_units;

void Response_Tabulator::Initialize_Lists(int k_points, int q_points)
{
	k_grid			   = libphysica::Log_Space(k_min, k_max, k_points);
	q_grid			   = libphysica::Log_Space(q_min, q_max, q_points);
	response_table	   = std::vector<std::vector<double>>(k_points, std::vector<double>(q_points, 0.0));
	tabulated_response = -1;
	electron_orbital   = "";
}

Response_Tabulator::Response_Tabulator(double kmin, double kmax, double qmin, double qmax)
: k_min(kmin), k_max(kmax), q_min(qmin), q_max(qmax)
{
	Initialize_Lists(100, 100);
}

void Response_Tabulator::Resize_Grid(int k_points, int q_points)
{
	if(q_points == 0)
		q_points = k_points;
	Initialize_Lists(k_points, q_points);
}

void Response_Tabulator::Tabulate(int response, const Initial_Electron_State& bound_electron, int threads)
{
	tabulated_response = response;
	electron_orbital   = bound_electron.Orbital_Name();

	std::cout << "\nTabulation of atomic response" << std::endl
			  << "\t- Electron orbital:\t\t" << bound_electron.Orbital_Name() << std::endl
			  << "\t- Response:\t\t\t" << response << std::endl
			  << "\t- Final momentum k'[keV]:\t[" << libphysica::Round(In_Units(k_min, keV)) << "," << libphysica::Round(In_Units(k_max, keV)) << "]" << std::endl
			  << "\t- Momentum transfer q[keV]:\t[" << libphysica::Round(In_Units(q_min, keV)) << "," << libphysica::Round(In_Units(q_max, keV)) << "]" << std::endl
			  << "\t- Grid size:\t\t\t" << k_grid.size() << "x" << q_grid.size() << std::endl
			  << "\t- Number of threads:\t\t" << threads << std::endl
			  << std::endl;

	int counter		= 0;
	int counter_max = k_grid.size() * q_grid.size();
#pragma omp parallel for schedule(dynamic) num_threads(threads) collapse(2)
	for(unsigned int ki = k_grid.size() - 1; ki >= 0; ki--)
		for(unsigned int qi = 0; qi < q_grid.size(); qi++)
		{
			double k			   = k_grid[ki];
			double q			   = q_grid[qi];
			response_table[ki][qi] = Atomic_Response_Function(k, q, bound_electron, response);
			counter++;
			libphysica::Print_Progress_Bar(1.0 * counter / counter_max, omp_get_thread_num());
		}
	std::cout << std::endl
			  << std::endl;
}

void Response_Tabulator::Export_Tables(const std::string& path)
{
	if(tabulated_response == -1)
	{
		std::cerr << "Error in Response_Tabulator::Export_Table(): Response has not been computed yet." << std::endl;
		std::exit(EXIT_FAILURE);
	}
	std::ofstream f_table, f_list;
	std::string path_table = path + electron_orbital + "_" + std::to_string(tabulated_response) + "_Table.txt";
	std::string path_list  = path + electron_orbital + "_" + std::to_string(tabulated_response) + "_List.txt";
	f_table.open(path_table);
	f_list.open(path_list);

	f_table << "// Table of W_" << tabulated_response << "(k',q) for " + electron_orbital << std::endl
			<< "// Rows correspond to values of k' [keV] in [" << In_Units(k_min, keV) << "," << In_Units(k_max, keV) << "] (log steps)" << std::endl
			<< "// Columns correspond to values of q [keV] in [" << In_Units(q_min, keV) << "," << In_Units(q_max, keV) << "] (log steps)" << std::endl;
	f_list << "// W_" << tabulated_response << "(k',q) for " + electron_orbital << std::endl
		   << "// k'[keV]\tq[keV]\tW_" << tabulated_response << std::endl;

	for(unsigned int ki = 0; ki < k_grid.size(); ki++)
	{
		double k = k_grid[ki];
		for(unsigned int qi = 0; qi < q_grid.size(); qi++)
		{
			double q = q_grid[qi];
			f_table << response_table[ki][qi];
			if(qi == q_grid.size() - 1)
				f_table << std::endl;
			else
				f_table << "\t";
			f_list << In_Units(k, keV) << "\t" << In_Units(q, keV) << "\t" << response_table[ki][qi] << std::endl;
		}
	}

	f_table.close();
	f_list.close();
	std::cout << "\n\nExported response W_" << tabulated_response << "(k',q) for " << electron_orbital << " as list and table to" << std::endl
			  << "\t" << path_list << std::endl
			  << "\t" << path_table << std::endl
			  << std::endl;
}

}	// namespace DarkARC

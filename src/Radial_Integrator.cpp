#include "DarkARC/Radial_Integrator.hpp"

#include <cmath>
#include <functional>

#include <boost/math/quadrature/gauss_kronrod.hpp>

#include "libphysica/Natural_Units.hpp"
#include "libphysica/Utilities.hpp"

#include "DarkARC/Special_Functions.hpp"

namespace DarkARC
{

using namespace libphysica::natural_units;

double Radial_Integrator::Radial_Integral_Adaptive(unsigned int integral_index, double k_final, double q, int l_final, int L)
{
	std::function<double(double)> integrand = [this, integral_index, L, q, k_final, l_final](double r) {
		switch(integral_index)
		{
			case 1:
				return r * r * initial_state.Radial_Wavefunction(r) * final_state->Radial_Wavefunction(r, k_final, l_final) * Spherical_Bessel_jL(L, q * r);
			case 2:
				return r * r * initial_state.Radial_Wavefunction_Derivative(r) * final_state->Radial_Wavefunction(r, k_final, l_final) * Spherical_Bessel_jL(L, q * r);
			case 3:
				return r * initial_state.Radial_Wavefunction(r) * final_state->Radial_Wavefunction(r, k_final, l_final) * Spherical_Bessel_jL(L, q * r);
			default:
				std::cerr << "Radial_Integrator::Radial_Integral_Adaptive(): Integral I_" << integral_index << " not defined." << std::endl;
				std::exit(EXIT_FAILURE);
		}
	};
	// Integrate stepwise
	double stepsize	 = 0.5 * Bohr_Radius;
	double integral	 = 0.0;
	double epsilon_1 = 1.0, epsilon_2 = 1.0;
	double tolerance = 1.0e-6;
	unsigned int i;
	for(i = 0; epsilon_1 > tolerance || epsilon_2 > tolerance; i++)
	{
		epsilon_2				= epsilon_1;
		double new_contribution = boost::math::quadrature::gauss_kronrod<double, 41>::integrate(integrand, i * stepsize, (i + 1) * stepsize, 5, 1e-9);

		integral += new_contribution;
		epsilon_1 = std::fabs(new_contribution / integral);
	}
	return integral;
}

double Radial_Integrator::Radial_Integral_Table(unsigned int integral_index, double k_final, double q, int l_final, int L)
{
	// Identify ki and qi
	int ki = Locate_Closest_Location(k_grid, k_final);
	int qi = Locate_Closest_Location(q_grid, q);

	// Check if the tables have been computed for l_final and L
	if(l_final > l_final_max[ki])
		Tabulate_Final_Wavefunction(l_final, ki);
	if(L > L_max[qi])
		Tabulate_Bessel_Function(L, qi);

	// Sum up the integral
	double integral = 0.0;
	for(unsigned int ri = 0; ri < r_values_and_weights.size(); ri++)
	{
		double r	  = r_values_and_weights[ri][0];
		double weight = r_values_and_weights[ri][1];

		if(integral_index == 1)
			integral += weight * r * r * initial_radial_wavefunction_list[ri] * final_radial_wavefunction_list[ki][l_final][ri] * bessel_function_list[qi][L][ri];
		else if(integral_index == 2)
			integral += weight * r * r * initial_radial_wavefunction_derivative_list[ri] * final_radial_wavefunction_list[ki][l_final][ri] * bessel_function_list[qi][L][ri];
		else if(integral_index == 3)
			integral += weight * r * initial_radial_wavefunction_list[ri] * final_radial_wavefunction_list[ki][l_final][ri] * bessel_function_list[qi][L][ri];
		else
		{
			std::cerr << "Radial_Integrator::Radial_Integral_Table(): Integral I_" << integral_index << " not defined." << std::endl;
			std::exit(EXIT_FAILURE);
		}
	}
	return integral;
}

Radial_Integrator::Radial_Integrator(const Initial_Electron_State& ini_state, const Final_Electron_State& fin_state)
: using_function_tabulation(false), initial_state(ini_state)
{
	final_state = fin_state.Clone();
}

void Radial_Integrator::Tabulate_Final_Wavefunction(int l_final, int ki)
{
	for(int li = l_final_max[ki] + 1; li <= l_final; li++)
	{
		std::vector<double> aux(r_points, 0.0);
		for(unsigned int ri = 0; ri < r_points; ri++)
			aux[ri] = final_state->Radial_Wavefunction(r_values_and_weights[ri][0], k_grid[ki], li);
		final_radial_wavefunction_list[ki].push_back(aux);
	}
	l_final_max[ki] = l_final;
}
void Radial_Integrator::Tabulate_Bessel_Function(int Lmax, int qi)
{
	for(int L = L_max[qi] + 1; L <= Lmax; L++)
	{
		std::vector<double> aux(r_points, 0.0);
		for(unsigned int ri = 0; ri < r_points; ri++)
			aux[ri] = Spherical_Bessel_jL(L, q_grid[qi] * r_values_and_weights[ri][0]);
		bessel_function_list[qi].push_back(aux);
	}
	L_max[qi] = Lmax;
}

void Radial_Integrator::Use_Tabulated_Functions(unsigned int rpoints, const std::vector<double>& k_list, const std::vector<double>& q_list)
{
	using_function_tabulation = true;
	k_grid					  = k_list;
	q_grid					  = q_list;
	l_final_max				  = std::vector<int>(k_grid.size(), -1);
	L_max					  = std::vector<int>(q_grid.size(), -1);

	r_points			 = rpoints;
	r_max				 = 100.0 * Bohr_Radius;
	r_values_and_weights = Compute_Gauss_Legendre_Roots_and_Weights(r_points, 0.0, r_max);

	// 1.  Tabulate initial radial wavefunctions
	initial_radial_wavefunction_list			= std::vector<double>(r_points, 0.0);
	initial_radial_wavefunction_derivative_list = std::vector<double>(r_points, 0.0);
	for(unsigned int ri = 0; ri < r_points; ri++)
	{
		initial_radial_wavefunction_list[ri]			= initial_state.Radial_Wavefunction(r_values_and_weights[ri][0]);
		initial_radial_wavefunction_derivative_list[ri] = initial_state.Radial_Wavefunction_Derivative(r_values_and_weights[ri][0]);
	}
	// 2. Initiate final radial wavefunctions and Bessel function list
	final_radial_wavefunction_list = std::vector<std::vector<std::vector<double>>>(k_grid.size(), std::vector<std::vector<double>>(0));
	bessel_function_list		   = std::vector<std::vector<std::vector<double>>>(q_grid.size(), std::vector<std::vector<double>>(0));
}

double Radial_Integrator::Radial_Integral(int integral_index, double k_final, double q, int l_final, int L)
{
	if(using_function_tabulation)
		return Radial_Integral_Table(integral_index, k_final, q, l_final, L);
	else
		return Radial_Integral_Adaptive(integral_index, k_final, q, l_final, L);
}

}	// namespace DarkARC
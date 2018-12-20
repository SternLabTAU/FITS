/*
 FITS - Flexible Inference from Time-Series data
 (c) 2016-2018 by Tal Zinger
 tal.zinger@outlook.com
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// CMulatorMain.cpp : Defines the entry point for the console application.
//

#define DEBUG_VERBOSE

#include <iostream>
#include <chrono>
#include <fstream>
#include <iomanip>

#include <regex> // for printing only executable name

#include <boost/format.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/io.hpp>
#include <boost/random/bernoulli_distribution.hpp>

#include "CMulator.h"
#include "clsCMulatorABC.h"

#include "PriorSampler.hpp"
#include "SimulationResult.hpp"
#include "ResultsStats.hpp"
#include "ActualDataFile.hpp"

#include "fits_constants.h"

int InferABC( FactorToInfer factor,
             std::string param_filename, std::string actual_data_filename,
             std::string posterior_output_filename, std::string summary_output_filename,
             std::string prior_output_filename )
{
    std::cout << "Parameter file: " << param_filename << std::endl;
    std::cout << "Data file: " << actual_data_filename << std::endl;
    std::cout << "Posterior distribution file: " << posterior_output_filename << std::endl;
    std::cout << "Summary file: " << summary_output_filename << std::endl;
    
    if ( prior_output_filename.compare("") != 0 ) {
        std::cout << "Prior distribution file: " << prior_output_filename << std::endl;
    }
    
    
    ZParams my_zparams;
    std::cout << "Reading parameters... ";
    try {
        // setting read-only to false, so we could add missing parameters
        my_zparams.ReadParameters(param_filename, false);
        std::cout << "Done." << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << "Exception while loading parameters: " << e.what() << std::endl;
        return 1;
    }
    catch (std::string str_exp) {
        std::cerr << "Exception while loading parameters: " << str_exp << std::endl;
        return 1;
    }
    catch (const char* str_exp) {
        std::cerr << "Exception while loading parameters: " << str_exp << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown exception while loading parameters." << std::endl;
        return 1;
    }
    
    bool is_multi_position = false;
    ActualDataFile actual_data_file;
    std::cout << "Reading data file... ";
    try {
        actual_data_file.LoadActualData(actual_data_filename);
        
        auto positions_detected = actual_data_file.GetNumberOfPositions();
        
        if (positions_detected>1) {
            std::cout << "Done - Multiple positions detected (" << positions_detected << ")." << std::endl;
            is_multi_position = true;
        }
        else {
            std::cout << "Done." << std::endl << std::endl;
        }
        
    }
    catch (std::string str) {
        std::cerr << "Exception while loading data: " << str << std::endl;
        return 1;
    }
    catch (const char* str) {
        std::cerr << "Exception while loading data: " << str << std::endl;
        return 1;
    }
    catch (std::exception& e) {
        std::cerr << "Exception while loading data: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown exception while loading data." << std::endl;
        return 1;
    }
    
    // Filling missing parameters from actual data
    try {
    if ( !my_zparams.IsParameterFound( fits_constants::PARAM_NUM_ALLELES ) ) {
        auto tmp_num_alleles = std::to_string( actual_data_file.GetNumberOfAlleles() );
        my_zparams.AddParameter( fits_constants::PARAM_NUM_ALLELES, tmp_num_alleles );

        std::cout << "Autodetected alleles: " << tmp_num_alleles << std::endl;
    }
    }
    catch (std::string str) {
        std::cerr << "Exception while filling out allele number. data: " << str << std::endl;
        return 1;
    }
    catch (const char* str) {
std::cerr << "Exception while filling out allele number. data: " << str << std::endl;
        return 1;
    }
    catch (std::exception& e) {
        std::cerr << "Exception while filling out allele number. data: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown exception while filling out allele number." << std::endl;
        return 1;
    }
    
    
    std::cout << std::endl << "Running simulations:" << std::endl;
    FLOAT_TYPE tmp_rejection_threshold = -1.0f;
    std::size_t running_time_sec = 0;
    std::vector<SimulationResult> accepted_results_vector;
    std::vector<SimulationResult> results_from_all_positions;
    
    PRIOR_DISTRIB used_prior_distrib;
    std::size_t results_to_accept = 0;
    PriorDistributionType prior_type = UNDEFINED;
    
    std::vector<FLOAT_TYPE> multi_pos_distance_vec;
    
    try {
        
        //auto positions_detected = actual_data_file.GetNumberOfPositions();
        
        //if ( positions_detected>1 ) {
        if ( is_multi_position ) {
            
            // multiple positions
            
            //std::vector< std::vector<FLOAT_TYPE>> global_prior(0);
            
            auto actual_positions_vec = actual_data_file.GetPositionNumbers();
            
            PRIOR_DISTRIB global_prior;
            
            for ( auto current_position_num : actual_positions_vec ) {
                
                auto current_position_data = actual_data_file.GetPosition(current_position_num);
                std::cout << "-- Position " << current_position_num << " --" << std::endl;
                
                /* ----------------- */
                /*  Run simulations  */
                /* ----------------- */
                clsCMulatorABC abc_object_sim( my_zparams, current_position_data, factor );
                
                
                // we want to use the prior generated for this position - for all of the rest
                
                if ( global_prior.empty() ) {
                    global_prior = abc_object_sim.GetPriorFloat();
                    used_prior_distrib = abc_object_sim.GetPriorFloat();
                }
                else {
                    abc_object_sim.SetPriorFloat(global_prior);
                }
                prior_type = abc_object_sim.GetPriorType();
                
                
                // results_to_accept = abc_object_sim.GetNumberOfKeptResults() * positions_detected;
                results_to_accept = abc_object_sim.GetNumberOfKeptResults();
                
                abc_object_sim.SetImmediateRejection(false);
                
                auto num_batches = my_zparams.GetInt(fits_constants::PARAM_SIM_REPEATS);
                
                if ( num_batches > 10 ) {
                    num_batches = 10;
                }
                
                abc_object_sim.RunABCInference(factor, num_batches);
                running_time_sec += abc_object_sim.GetRunningTimeSec();
                tmp_rejection_threshold = abc_object_sim.GetRejectionThreshold();
                
                /* --------------- */
                /*  Store results  */
                /* --------------- */
                std::cout << "Aggregating multi-polsition data... ";
                auto position_results_vec = abc_object_sim.GetResultsVector(false);
                
                // add distances - make sure data is consistent
                if ( accepted_results_vector.empty() ) {
                    
                    accepted_results_vector.resize( global_prior.size() );
                    // std::cout << "accepted results initialized to size " << accepted_results_vector.size() << std::endl;
                    // std::cout << "position results vector size " << position_results_vec.size() << std::endl;
                    
                    //for ( auto result_idx=0; result_idx<tmp_accepted_results_vec.size(); ++result_idx ) {
                    for ( auto current_result : position_results_vec ) {
                        
                        auto result_idx = current_result.prior_sample_index;
                        //std::cout << "prior sample index:" << result_idx << std::endl;
                        
                        current_result.SetMultiPosition(true);
                        current_result.pos = current_position_num;
                        current_result.sum_distance = current_result.distance_from_actual;
                        //std::cout << accepted_results_vector[result_idx].sum_distance;
                        accepted_results_vector[result_idx] = current_result;
                        
                        //tmp_accepted_results_vec[result_idx].SetMultiPosition(true);
                        //tmp_accepted_results_vec[result_idx].pos = current_position_num;
                        //tmp_accepted_results_vec[result_idx].sum_distance = tmp_accepted_results_vec[result_idx].distance_from_actual;
                        
                        //accepted_results_vector.push_back( tmp_accepted_results_vec[result_idx] );
                        // accepted_results_vector[result_idx] = tmp_accepted_results_vec[result_idx];
                        
                        
                        results_from_all_positions.push_back( current_result );
                    }
                }
                else {
                    //for ( auto result_idx=0; result_idx<tmp_accepted_results_vec.size(); ++result_idx ) {
                    // std::cout << "position results vector size " << position_results_vec.size() << std::endl;
                    
                    for ( auto current_result : position_results_vec ) {
                        
                        auto result_idx = current_result.prior_sample_index;
                        //std::cout << "prior sample index:" << result_idx << std::endl;
                        
                        // test consistency
                        // std::cout << "\n prior idx " << result_idx << " stored: ";
                        // for ( auto val : accepted_results_vector[result_idx].fitness_values ) std::cout << val << ",";
                        // std::cout << " with distance=" << accepted_results_vector[result_idx].sum_distance << " adding " << current_result.distance_from_actual << std::endl;
                        // std::cout << " for fitness: ";
                        // for ( auto val : current_result.fitness_values ) std::cout << val << ",";
                        
                        current_result.SetMultiPosition(true);
                        current_result.pos = current_position_num;
                        accepted_results_vector[result_idx].sum_distance += current_result.distance_from_actual;
                        
                        results_from_all_positions.push_back( current_result );
                        
                        //tmp_accepted_results_vec[result_idx].SetMultiPosition(true);
                        //tmp_accepted_results_vec[result_idx].pos = current_position_num;
                        
                        //accepted_results_vector[result_idx].sum_distance = accepted_results_vector[result_idx].sum_distance + tmp_accepted_results_vec[result_idx].distance_from_actual;
                        
                        // std::cout << "\t new distance=" << accepted_results_vector[result_idx].sum_distance << std::endl;
                    }
                }
                std::cout << "Done." << std::endl;
            }
            
            std::cout << "Finished running simulations." << std::endl << std::endl;
            
            std::cout << "Testing prior coverage... ";
            std::vector<std::size_t> prior_idx_vec;
            for ( auto current_result : accepted_results_vector ) {
                prior_idx_vec.push_back( current_result.prior_sample_index );
            }
            std::sort( prior_idx_vec.begin(), prior_idx_vec.end() );
            std::cout << "found " << prior_idx_vec.size() << " samples, ";
            for ( auto current_idx = 0; current_idx < prior_idx_vec.size(); ++current_idx ) {
                if ( current_idx != prior_idx_vec[current_idx] ) {
                    std::cout << "problem at idx " << current_idx << " value " << prior_idx_vec[current_idx] << std::endl;
                    throw "prior not complete";
                }
                
            }
            std::cout << "all values found. Done." << std::endl;
            
            std::cout << "Sorting data from multiple positions... ";
            std::nth_element(accepted_results_vector.begin(),
                             accepted_results_vector.begin() + results_to_accept,
                             accepted_results_vector.end());
            
            accepted_results_vector.erase( accepted_results_vector.begin() + results_to_accept, accepted_results_vector.end() );
            
            std::sort( accepted_results_vector.begin(), accepted_results_vector.end() );
            
            std::cout << "Done." << std::endl;
        }
        else {
            
            // single position
            
            /* ----------------- */
            /*  Run simulations  */
            /* ----------------- */
            clsCMulatorABC abc_object_sim( my_zparams, actual_data_file.GetFirstPosition(), factor );
            
            prior_type = abc_object_sim.GetPriorType();
            
            abc_object_sim.SetImmediateRejection(false);
            
            auto num_batches = my_zparams.GetInt(fits_constants::PARAM_SIM_REPEATS);
            
            if ( num_batches > 10 ) {
                num_batches = 10;
            }
            
            abc_object_sim.RunABCInference(factor, num_batches);
            running_time_sec += abc_object_sim.GetRunningTimeSec();
            tmp_rejection_threshold = abc_object_sim.GetRejectionThreshold();
            
            std::cout << "Testing prior coverage... ";
            auto tmp_all_results_vec = abc_object_sim.GetResultsVector(false);
            std::vector<std::size_t> prior_idx_vec;
            for ( auto current_result : tmp_all_results_vec ) {
                prior_idx_vec.push_back( current_result.prior_sample_index );
            }
            std::sort( prior_idx_vec.begin(), prior_idx_vec.end() );
            std::cout << "found " << prior_idx_vec.size() << " samples, ";
            for ( auto current_idx = 0; current_idx < prior_idx_vec.size(); ++current_idx ) {
                if ( current_idx != prior_idx_vec[current_idx] ) {
                    std::cout << "problem at idx " << current_idx << " value " << prior_idx_vec[current_idx] << std::endl;
                    throw "prior not complete";
                }
                
            }
            std::cout << "all values found. Done." << std::endl;
            
            /* --------------- */
            /*  Store results  */
            /* --------------- */
            std::cout << "Gathering results... ";
            auto tmp_accepted_results_vec = abc_object_sim.GetResultsVector(true);
            for ( auto tmp_result : tmp_accepted_results_vec ) {
                accepted_results_vector.push_back(tmp_result);
            }
            
            std::sort( accepted_results_vector.begin(), accepted_results_vector.end() );
            
            used_prior_distrib = abc_object_sim.GetPriorFloat();
            std::cout << "Done." << std::endl;
            
            std::cout << "Finished running simulations." << std::endl << std::endl;
        }
        
        
        /* --------------------- */
        /*  Process simulations  */
        /* --------------------- */
        std::cout << "Processing results (" <<  accepted_results_vector.size() << " simulations):" << std::endl;
        
        
        
        ResultsStats result_stats( my_zparams, prior_type, used_prior_distrib, accepted_results_vector );
        
        result_stats.SetPriorType(prior_type);
        
        // result_stats.SetRejectionThreshold( abc_object_sim.GetRejectionThreshold() );
        result_stats.SetRejectionThreshold( tmp_rejection_threshold );
        
        auto single_mutation_rate = my_zparams.GetFloat( fits_constants::PARAM_SINGLE_MUTATION_RATE, 0.0f );
        result_stats.SetSingleMutrateUsed( single_mutation_rate != 0.0f );
        
        // result_stats.SetRunningTimeSec( abc_object_sim.GetTotalRunningTimeSec() );
        result_stats.SetRunningTimeSec( running_time_sec );
        
        
        
        switch (factor) {
            case Fitness: {
                
                try {
                    //result_stats.SetPriorType( my_zparams.GetString(fits_constants::PARAM_PRIOR_DISTRIB, fits_constants::PARAM_PRIOR_DISTRIB_DEFAULT ) );
                    //result_stats.CalculateStatsFitness(accepted_results_vector);
                    //result_stats.SetPriorDistrib(used_prior_distrib);
                    result_stats.CalculateStatsFitness();
                }
                catch (std::string str) {
                    std::cerr << "Exception caught: " << str << std::endl;
                    return 1;
                }
                catch (const char* str) {
                    std::cerr << "Exception caught: " << str << std::endl;
                    return 1;
                }
                catch (std::exception& e) {
                    std::cerr << "Exception caught: " << e.what() << std::endl;
                    return 1;
                }
                catch (...) {
                    throw "Error while calculating stats.";
                }
                
                
                try {
                    std::string tmp_summary_str = result_stats.GetSummaryFitness(true);
                    
                    //void ResultsStats::WriteMultiPositionPosterior( FactorToInfer factor, const std::vector<SimulationResult>& accepted_results_vec, const std::vector<SimulationResult>& all_results_vec, std::string filename )

                    
                    std::cout << "Writing posterior... ";
                    //result_stats.WriteFitnessDistribToFile(accepted_results_vector, posterior_output_filename);
                    result_stats.WritePosterior( is_multi_position, FactorToInfer::Fitness, accepted_results_vector, results_from_all_positions, posterior_output_filename );
                    
                    std::cout << "Done." << std::endl;
                
                    std::cout << "Writing summary... ";
                    result_stats.WriteStringToFile(summary_output_filename, tmp_summary_str);
                    std::cout << "Done." << std::endl;
                    
                    if ( prior_output_filename.compare("") != 0 ) {
                        std::cout << "Writing prior... ";
                        result_stats.WritePriorDistribToFile( factor, used_prior_distrib, prior_output_filename);
                        std::cout << "Done." << std::endl;
                    }
                    
                    std::cout << std::endl << "Summary:" << std::endl;
                    std::cout << tmp_summary_str << std::endl;
                }
                catch (std::string str) {
                    std::cerr << "Exception caught: " << str << std::endl;
                    return 1;
                }
                catch (const char* str) {
                    std::cerr << "Exception caught: " << str << std::endl;
                    return 1;
                }
                catch (std::exception& e) {
                    std::cerr << "Exception caught: " << e.what() << std::endl;
                    return 1;
                }
                catch (...) {
                    throw "Error writing results files.";
                }
                
                break;
            }
                
                
            case PopulationSize: {
                
                try {
                    //result_stats.SetPriorType( my_zparams.GetString(fits_constants::PARAM_PRIOR_DISTRIB, fits_constants::PARAM_PRIOR_DISTRIB_UNIFORM ) );
                    //result_stats.SetPriorDistrib(used_prior_distrib);
                    //result_stats.CalculateStatsFitness(accepted_results_vector);
                    //result_stats.CalculateStatsPopulationSize(accepted_results_vector);
                    result_stats.CalculateStatsPopulationSize();
                }
                catch (std::string str) {
                    std::cerr << "Exception caught: " << str << std::endl;
                    return 1;
                }
                catch (const char* str) {
                    std::cerr << "Exception caught: " << str << std::endl;
                    return 1;
                }
                catch (std::exception& e) {
                    std::cerr << "Exception caught: " << e.what() << std::endl;
                    return 1;
                }
                catch (...) {
                    throw "Error while calculating stats.";
                }
                
                
                
                try {
                    std::string tmp_summary_str = result_stats.GetSummaryPopSize(true);
                    
                    
                    std::cout << "Writing posterior... ";
                    result_stats.WritePosterior( is_multi_position, FactorToInfer::PopulationSize, accepted_results_vector, results_from_all_positions, posterior_output_filename );
                    
                    //result_stats.WritePopSizeDistribToFile(accepted_results_vector, posterior_output_filename);
                    std::cout << "Done." << std::endl;
                
                    std::cout << "Writing summary... ";
                    result_stats.WriteStringToFile(summary_output_filename, tmp_summary_str);
                    std::cout << "Done." << std::endl;
                    
                    if ( prior_output_filename.compare("") != 0 ) {
                        std::cout << "Writing prior... ";
                        result_stats.WritePriorDistribToFile( factor, used_prior_distrib, prior_output_filename);
                        std::cout << "Done." << std::endl;
                    }
                    
                    std::cout << std::endl << "Summary:" << std::endl;
                    std::cout << tmp_summary_str << std::endl;
                }
                catch (const char* str) {
                    std::cerr << "Exception caught: " << str << std::endl;
                    return 1;
                }
                catch (std::string str) {
                    std::cerr << "Exception caught: " << str << std::endl;
                    return 1;
                }
                catch (std::exception& e) {
                    std::cerr << "Exception caught: " << e.what() << std::endl;
                    return 1;
                }
                catch (...) {
                    throw "Error writing results files.";
                }
                
                break;
            }
                
            case MutationRate: {
                
                auto infer_single_mutrate = my_zparams.GetInt(fits_constants::PARAM_MIN_LOG_SINGLE_MUTATION_RATE, 0);
                
                try {
                    result_stats.SetSingleMutrateInferred( infer_single_mutrate != 0 );
                    //result_stats.SetPriorType( my_zparams.GetString(fits_constants::PARAM_PRIOR_DISTRIB, fits_constants::PARAM_PRIOR_DISTRIB_UNIFORM ) );
                    //result_stats.SetPriorDistrib(used_prior_distrib);
                    //result_stats.CalculateStatsMutation(accepted_results_vector);
                    result_stats.CalculateStatsMutation();
                }
                catch (std::string str) {
                    std::cerr << "Exception caught: " << str << std::endl;
                    return 1;
                }
                catch (const char* str) {
                    std::cerr << "Exception caught: " << str << std::endl;
                    return 1;
                }
                catch (std::exception& e) {
                    std::cerr << "Exception caught: " << e.what() << std::endl;
                    return 1;
                }
                catch (...) {
                    throw "Error while calculating stats.";
                }
                
                
                try {
                    std::string tmp_summary_str = result_stats.GetSummaryMutRate(true);
                    
                    std::cout << "Writing posterior... ";
                    // result_stats.WriteMutRateDistribToFile(accepted_results_vector, posterior_output_filename);
                    result_stats.WritePosterior( is_multi_position, FactorToInfer::MutationRate, accepted_results_vector, results_from_all_positions, posterior_output_filename );
                    
                    std::cout << "Done." << std::endl;
                    
                    std::cout << "Writing summary... ";
                    result_stats.WriteStringToFile(summary_output_filename, tmp_summary_str);
                    std::cout << "Done." << std::endl;
                    
                    if ( prior_output_filename.compare("") != 0 ) {
                        std::cout << "Writing prior... ";
                        result_stats.WritePriorDistribToFile( factor, used_prior_distrib, prior_output_filename);
                        std::cout << "Done." << std::endl;
                    }
                    
                    std::cout << std::endl << "Summary:" << std::endl;
                    std::cout << tmp_summary_str << std::endl;
                }
                catch (const char* str) {
                    std::cerr << "Exception caught: " << str << std::endl;
                    return 1;
                }
                catch (std::string str) {
                    std::cerr << "Exception caught: " << str << std::endl;
                    return 1;
                }
                catch (std::exception& e) {
                    std::cerr << "Exception caught: " << e.what() << std::endl;
                    return 1;
                }
                catch (...) {
                    throw "Error writing results files.";
                }
                
                break;
            }
        }
    }
    catch (const char* str) {
        std::cerr << "Exception caught while attempting to report stats: " << str << std::endl;
        return 1;
    }
    catch (std::string str) {
        std::cerr << "Exception caught while attempting to report stats: " << str << std::endl;
        return 1;
    }
    catch (std::exception& e) {
        std::cerr << "Exception caught while attempting to report stats: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown exception while attempting to report stats." << std::endl;
        return 1;
    }
    
    
    return 0;
}


int RunSingleSimulation(std::string param_filename, std::string output_filename)
{
    // simulator object
    CMulator sim_object;
    ZParams my_zparams;
    
    try {
        my_zparams.ReadParameters(param_filename, true);
        
        sim_object.InitMemberVariables(my_zparams);
        
        if ( !sim_object.IsAbleToSimulate() ) {
            throw "Missing parameters for simulation.";
        }
        
        //sim_object.ReadParametersFromFile(param_filename);
        // sim_object.InitializeFromParamFile(param_filename);
    }
    catch (const char* txt) {
        std::cerr << "Exception caught while initializing simulator object:" << std::endl;
        std::cerr << txt << std::endl;
        return 1;
    }
    catch (std::string txt) {
        std::cerr << "Exception caught while initializing simulator object:" << std::endl;
        std::cerr << txt << std::endl;
        return 1;
    }
    catch (std::exception& e) {
        std::cerr << "Exception caught while initializing simulator object:" << std::endl;
        std::cerr << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown exception while initializing simulator object." << std::endl;
        throw;
        return 1;
    }
    
    std::cout << "Simulator object initialized." << std::endl;
    std::cout << "Random seed set to " << sim_object.GetRandomSeed() << std::endl;
    std::cout << "Running simulation..." << std::endl;
    try {
        sim_object.EvolveAllGenerations();
    }
    catch (const char* txt) {
        std::cerr << "Exception: " << txt << std::endl;
    }
    std::cout << "Done." << std::endl;
    
    //std::string sim_output = sim_object.GetAllOutputAsText();
    std::string sim_output = sim_object.GetAllOutputAsTextForR(true);
    
    // also output to screen
    // TODO: make this via parameter
    std::cout << sim_output << std::endl;
    
    std::cout << std::endl;
    
    //std::cout << sim_object.GetAllOutputAsMatrix() << std::endl;
    
    std::ofstream outfile(output_filename, std::ofstream::out | std::ofstream::trunc);
    
    if (!outfile.is_open()) {
        std::cerr << "unable to open file for writing: " << output_filename << std::endl;
        return 1;
    }
    std::cout << "Writing to file...";
    outfile << sim_output;
    outfile.close();
    std::cout << " Done." << std::endl;
    
    return 0;
}




void print_syntaxes(std::string exec_name)
{
    exec_name = "fits ";
    std::cout << "\t" << exec_name << fits_constants::ARG_INFER_FITNESS
    << " <param_file> <actual_data_file> <posterior_file> <summary_file> (optional: <prior_file>)" << std::endl << std::endl;
    
    std::cout << "\t" << exec_name << fits_constants::ARG_INFER_MUTATION
    << " <param_file> <actual_data_file> <posterior_file> <summary_file> (optional: <prior_file>)" << std::endl << std::endl;
    
    std::cout << "\t" << exec_name << fits_constants::ARG_INFER_POPSIZE
    << " <param_file> <actual_data_file> <posterior_file> <summary_file> (optional: <prior_file>)" << std::endl << std::endl;
    
    std::cout << "\t" << exec_name << "-simulate <param_file> <output_file>" << std::endl << std::endl;
}


void print_welcome()
{
    std::cout << std::endl << "================================================";
    std::cout << std::endl << "                  FITS v" << fits_constants::current_version_str;
    std::cout << std::endl << "    Flexible Inference from Time-Series data    ";
    std::cout << std::endl << "         (c) Tal Zinger, Stern Lab, TAU         ";
    std::cout << std::endl << "================================================";
    std::cout << std::endl;
}

bool IsInferenceRun( std::string first_argument )
{
    return ( first_argument.compare( fits_constants::ARG_INFER_FITNESS ) == 0
            || first_argument.compare( fits_constants::ARG_INFER_MUTATION ) == 0
            || first_argument.compare( fits_constants::ARG_INFER_POPSIZE ) == 0 );
}


int main(int argc, char* argv[])
{
    
    if (argc <= 1) {
        print_welcome();
        print_syntaxes(argv[0]);
        return 1;
    }
    
    std::string tmp_first_param = argv[1];
    
    std::string param_filename = "";
    std::string actual_data_filename = "";
    std::string posterior_output_filename = "";
    std::string summary_output_filename = "";
    std::string prior_output_filename = "";
    
    if ( IsInferenceRun(tmp_first_param) ) {
        
        // prior is optional
        if (argc != 6 && argc != 7) {
            std::cout << "illegal number of arguments (" << argc-1 << "). Syntax is:" << std::endl;
            print_syntaxes(argv[0]);
            return 1;
        }
        
        param_filename = argv[2];
        actual_data_filename = argv[3];
        posterior_output_filename = argv[4];
        summary_output_filename = argv[5];
        
        if ( argc == 7 ) {
            prior_output_filename = argv[6];
        }
    }
    
    if ( tmp_first_param.compare( fits_constants::ARG_INFER_FITNESS ) == 0 ) {
        
        print_welcome();
        
        std::cout << "Inferring fitness" << std::endl;
        
        return InferABC( FactorToInfer::Fitness,
                        param_filename, actual_data_filename,
                        posterior_output_filename, summary_output_filename,
                        prior_output_filename);
    }
    
    if ( tmp_first_param.compare( fits_constants::ARG_INFER_MUTATION ) == 0 ) {
        
        print_welcome();
        
        std::cout << "Inferring mutation rate" << std::endl;
        
        return InferABC( FactorToInfer::MutationRate,
                        param_filename, actual_data_filename,
                        posterior_output_filename, summary_output_filename,
                        prior_output_filename);
    }
    
    if ( tmp_first_param.compare( fits_constants::ARG_INFER_POPSIZE ) == 0 ) {
        
        print_welcome();
        
        std::cout << "Inferring population size" << std::endl;
        
        return InferABC( FactorToInfer::PopulationSize,
                        param_filename, actual_data_filename,
                        posterior_output_filename, summary_output_filename,
                        prior_output_filename );
    }
    
    if (tmp_first_param == "-simulate") {
        
        print_welcome();
        
        if (argc != 4) {
            std::cout << "illegal number of arguments (" << argc-1 << "). Syntax is:" << std::endl;
            print_syntaxes(argv[0]);
            return 1;
        }
        
        std::cout << "Running a single simulation" << std::endl;
        
        param_filename = argv[2];
        std::string output_filename = argv[3];
        
        return RunSingleSimulation(param_filename, output_filename);
    }
    
    
    // we should never reach here...
    print_welcome();
    std::cout << "Invalid suntax (option chosen was " << tmp_first_param << "). Use only the following:" << std::endl;
    print_syntaxes(argv[0]);
    return 1;
}


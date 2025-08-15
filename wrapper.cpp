// This is a wrapper for libsbmlsim so it's easier to use with JS.
#include <string>
#include <cstdio>
#include <cstdlib>
#include <map>

#include <emscripten/bind.h>
#include <libsbmlsim/libsbmlsim.h>

using namespace emscripten;

struct Column {
    std::string name;
    std::vector<double> values;

    Column() : name(""), values() {};
    Column(const std::string &name, const std::vector<double> &values)
        : name(name), values(values) {}
};

class Result {
  public:
    std::vector<Column> columns;

    // Takes ownership of the myResult.
    Result(myResult *result) {
        columns.reserve(
            1 + result->num_of_columns_sp +
            result->num_of_columns_param);
        
        columns.emplace_back(
            std::string{result->column_name_time},
            std::vector<double>(result->values_time, result->values_time + result->num_of_rows)
        );

        // add species columns
        for (int i = 0; i < result->num_of_columns_sp; ++i) {
            columns.emplace_back(
                result->column_name_sp[i],
                std::vector<double>(result->num_of_rows)
            );
        }

        double *value_sp = result->values_sp;
        for (int i = 0; i < result->num_of_rows; ++i) {
            for (int j = 0; j < result->num_of_columns_sp; ++j) {
                int col_index = columns.size() - result->num_of_columns_sp + j;
                columns[col_index].values[i] = *(value_sp++);
            }
        }

        // add parameter columns
        for (int i = 0; i < result->num_of_columns_param; ++i) {
            columns.emplace_back(
                result->column_name_param[i],
                std::vector<double>(result->num_of_rows)
            );
        }

        double *values_param = result->values_param;
        for (int i = 0; i < result->num_of_rows; ++i) {
            for (int j = 0; j < result->num_of_columns_param; ++j) {
                int col_index = columns.size() - result->num_of_columns_param + j;
                columns[col_index].values[i] = *(values_param++);
            }
        }

        free_myResult(result);
    }

    // For testing
    void Print() {
        for (const auto &col : columns) {
            printf("%s ", col.name.c_str());
        }
        printf("\n");
        for (int i = 0; i < columns[0].values.size(); ++i) {
            for (const auto &col : columns) {
                printf("%f ", col.values[i]);
            }
            printf("\n");
        }
    }
};

class Simulator {
  public:
    Simulator() {};
    ~Simulator() {
        if (document_) {
            SBMLDocument_free(document_);
        }
    }

    // Use this to get an error to display to the user when
    // an operation fails such as `LoadSbml`.
    std::string GetLastError() {
        return last_error_;
    }

    // Loads an SBML string into the simulator.
    // You can then simulate using `simulateTimeCourse`.
    // Returns true on success, false on error.
    bool LoadSbml(const std::string &sbml_string) {
        SBMLDocument_t *d;
        Model_t *m;
        myResult *rtn;

        d = readSBMLFromString(sbml_string.c_str());
        if (d == nullptr) {
            last_error_ = "Failed to read SBML.";
            return false;
        }

        int err_num = SBMLDocument_getNumErrors(d);
        if (err_num > 0) {
            const XMLError_t *err = static_cast<const XMLError_t *>(
                SBMLDocument_getError(d, 0)
            );
            if (XMLError_isError(err) || XMLError_isFatal(err)) {
                auto err_code = XMLError_getErrorId(err);
                switch (err_code) {
                    case XMLFileUnreadable:
                        last_error_ = "File not found.";
                        break;
                    case XMLFileUnwritable:
                    case XMLFileOperationError:
                    case XMLNetworkAccessError:
                        last_error_ = "SBML operation failed.";
                        break;
                    case InternalXMLParserError:
                    case UnrecognizedXMLParserCode:
                    case XMLTranscoderError:
                        last_error_ = "Internal parser error for SBML.";
                        break;
                    case XMLOutOfMemory:
                        last_error_ = "Out of memory.";
                        break;
                    case XMLUnknownError:
                        last_error_ = "Unknown error in SBML.";
                        break;
                    default:
                        last_error_ = "Invalid SBML.";
                        break;
                }

                SBMLDocument_free(d);
                return false;
            }
        }

        return SetDocument(d);
    }

    // Returns map of floating species and initial concentrations.
    std::map<std::string, double> GetFloatingSpecies() {
        if (document_ == nullptr) {
            last_error_ = "No SBML document loaded.";
            return {};
        }

        Model_t *m = SBMLDocument_getModel(document_);

        std::map<std::string, double> species;
        int num_species = Model_getNumSpecies(m);
        for (int i = 0; i < num_species; ++i) {
            Species_t *s = Model_getSpecies(m, i);
            if (s != nullptr && !Species_getBoundaryCondition(s)) {
                auto id = Species_getId(s);
                species[id] = initial_species_concentrations_[id];
            }
        }

        return species;
    }

    // Returns map of boundary species and initial concentrations.
    std::map<std::string, double> GetBoundarySpecies() {
        if (document_ == nullptr) {
            last_error_ = "No SBML document loaded.";
            return {};
        }

        Model_t *m = SBMLDocument_getModel(document_);

        std::map<std::string, double> species;
        int num_species = Model_getNumSpecies(m);
        for (int i = 0; i < num_species; ++i) {
            Species_t *s = Model_getSpecies(m, i);
            if (s != nullptr && Species_getBoundaryCondition(s)) {
                auto id = Species_getId(s);
                species[id] = initial_species_concentrations_[id];
            }
        }

        return species;
    }

    // Returns map of parameters and initial values.
    std::map<std::string, double> GetParameters() {
        if (document_ == nullptr) {
            last_error_ = "No SBML document loaded.";
            return {};
        }

        Model_t *m = SBMLDocument_getModel(document_);

        std::map<std::string, double> parameters;
        int num_params = Model_getNumParameters(m);
        for (int i = 0; i < num_params; ++i) {
            Parameter_t *p = Model_getParameter(m, i);
            if (p != nullptr) {
                auto id = Parameter_getId(p);
                parameters[id] = initial_parameter_values_[id];
            }
        }

        return parameters;
    }

    std::unique_ptr<Result> SimulateTimeCourse(double time, int num_points) {
        if (document_ == nullptr) {
            last_error_ = "No SBML document loaded.";
            return nullptr;
        }
        
        double dt = time / static_cast<double>(num_points); // time step size
        double print_interval = dt; // print at every time step
        bool should_print_amount = true;
        // No idea what these are for
        bool use_lazy_method = false;
        double atol = 0.0;
        double rtol = 0.0;
        double facmax = 0.0;

        Model_t *m = SBMLDocument_getModel(document_);

        myResult *result = simulateSBMLModel(
            m, time, dt, print_interval, should_print_amount, MTHD_RUNGE_KUTTA_FEHLBERG_5,
            use_lazy_method, atol, rtol, facmax
        );
        if (result == nullptr) {
            last_error_ = "Simulation failed.";
            return nullptr;
        } else if (myResult_isError(result)) {
            last_error_ = myResult_getErrorMessage(result);
            free_myResult(result);
            return nullptr;
        }

        return std::make_unique<Result>(result);
    }

    void SetVariable(const std::string &name, double value) {
        if (document_ == nullptr) {
            last_error_ = "No SBML document loaded.";
            return;
        }

        Model_t *m = SBMLDocument_getModel(document_);
        Species_t *species = Model_getSpeciesById(m, name.c_str());
        if (species != nullptr) {
            Species_setInitialConcentration(species, value);
            return;
        }

        Parameter_t *parameter = Model_getParameterById(m, name.c_str());
        if (parameter != nullptr) {
            Parameter_setValue(parameter, value);
            return;
        }
    }

    void ResetVariables() {
        if (document_ == nullptr) {
            last_error_ = "No SBML document loaded.";
            return;
        }

        Model_t *m = SBMLDocument_getModel(document_);

        for (const auto &[id, value] : initial_species_concentrations_) {
            Species_t *species = Model_getSpeciesById(m, id.c_str());
            if (species != nullptr) {
                Species_setInitialConcentration(species, value);
            }
        }

        for (const auto &[id, value] : initial_parameter_values_) {
            Parameter_t *parameter = Model_getParameterById(m, id.c_str());
            if (parameter != nullptr) {
                Parameter_setValue(parameter, value);
            }
        }
    }

  private:
    std::string last_error_ = "";
    SBMLDocument_t *document_ = nullptr;

    std::map<std::string, double> initial_species_concentrations_;
    std::map<std::string, double> initial_parameter_values_;

    bool SetDocument(SBMLDocument_t *d) {
        Model_t *m = SBMLDocument_getModel(d);
        if (m == nullptr) {
            last_error_ = "No model found in SBML document.";
            return false;
        }

        document_ = d;
        initial_species_concentrations_.clear();
        initial_parameter_values_.clear();

        // set initial values for species and parameters
        int num_species = Model_getNumSpecies(m);
        for (int i = 0; i < num_species; ++i) {
            Species_t *s = Model_getSpecies(m, i);
            if (s != nullptr && !Species_getBoundaryCondition(s)) {
                initial_species_concentrations_[Species_getId(s)] = Species_getInitialConcentration(s);
            }
        }
        
        int num_params = Model_getNumParameters(m);
        for (int i = 0; i < num_params; ++i) {
            Parameter_t *p = Model_getParameter(m, i);
            if (p != nullptr) {
                initial_parameter_values_[Parameter_getId(p)] = Parameter_getValue(p);
            }
        }

        return true;
    }
};

EMSCRIPTEN_BINDINGS(libsbmlsim_wrapper) {
    register_vector<double>("VectorDouble");
    register_vector<Column>("VectorColumn");
    register_vector<std::string>("VectorString");
    register_map<std::string, double>("MapStringDouble");

    value_object<Column>("Column")
        .field("name", &Column::name)
        .field("values", &Column::values);

    class_<Result>("Result")
        .function("Print", &Result::Print)
        .property("columns", &Result::columns);

    class_<Simulator>("Simulator")
        .constructor<>()
        .function("GetLastError", &Simulator::GetLastError)
        .function("LoadSbml", &Simulator::LoadSbml)
        .function("GetFloatingSpecies", &Simulator::GetFloatingSpecies)
        .function("GetBoundarySpecies", &Simulator::GetBoundarySpecies)
        .function("GetParameters", &Simulator::GetParameters)
        .function("SetVariable", &Simulator::SetVariable)
        .function("ResetVariables", &Simulator::ResetVariables)
        .function("SimulateTimeCourse", &Simulator::SimulateTimeCourse);
}
#include <deque>
#include <iostream>
#include <string>
#include <tclap/CmdLine.h>

#include "Pore_Model.hpp"
#include "Builtin_Model.hpp"
#include "State_Transitions.hpp"
#include "Event.hpp"
#include "Fast5_Summary.hpp"
#include "Viterbi.hpp"
#include "Forward_Backward.hpp"
#include "logger.hpp"
#include "zstr.hpp"
#include "fast5.hpp"

using namespace std;

typedef Pore_Model<> Pore_Model_Type;
typedef Pore_Model_Parameters<> Pore_Model_Parameters_Type;
typedef State_Transitions<> State_Transitions_Type;
typedef Event<> Event_Type;
typedef Event_Sequence<> Event_Sequence_Type;
typedef Fast5_Summary<> Fast5_Summary_Type;

typedef map< string, Pore_Model_Type > Model_Dict_Type;

namespace opts
{
    using namespace TCLAP;
    string description = "Call bases in ONT reads";
    CmdLine cmd_parser(description);
    MultiArg< string > log_level("", "log", "Log level.", false, "string", cmd_parser);
    MultiArg< string > model_fn("m", "model", "Pore model.", false, "file", cmd_parser);
    ValueArg< string > model_fofn("", "model-fofn", "File of pore models.", false, "", "file", cmd_parser);
    ValueArg< string > trans_fn("s", "trans", "Initial state transitions.", false, "", "file", cmd_parser);
    ValueArg< string > output_fn("o", "output", "Output.", false, "", "file", cmd_parser);
    ValueArg< float > pr_stay("", "pr-stay", "Transition probability of staying in the same state.", false, .1, "float", cmd_parser);
    ValueArg< float > pr_skip("", "pr-skip", "Transition probability of skipping at least 1 state.", false, .1, "float", cmd_parser);
    ValueArg< float > pr_cutoff("", "pr-cutoff", "Minimum value for transition probabilities; smaller values are set to zero.", false, .001, "float", cmd_parser);
    UnlabeledMultiArg< string > input_fn("inputs", "Input files/directories", true, "path", cmd_parser);
} // namespace opts

void init_models(Model_Dict_Type& models)
{
    auto parse_model_name = [] (const string& s) {
        if (s.size() < 3
            or (s[0] != '0' and s[0] != '1' and s[0] != '2')
            or s[1] != ':')
        {
            cerr << "could not parse model name: \"" << s << "\"; format should be \"[0|1|2]:<file>\"" << endl;
            exit(EXIT_FAILURE);
        }
        unsigned st = s[0] - '0';
        return make_pair(st, s.substr(2));
    };

    map< unsigned, list< string > > model_list;
    if (not opts::model_fn.get().empty())
    {
        for (const auto& s : opts::model_fn.get())
        {
            auto p = parse_model_name(s);
            model_list[p.first].push_back(p.second);
        }
    }
    if (not opts::model_fofn.get().empty())
    {
        strict_fstream::ifstream ifs(opts::model_fofn);
        string s;
        while (getline(ifs, s))
        {
            auto p = parse_model_name(s);
            model_list[p.first].push_back(p.second);
        }
    }
    if (model_list[2].empty() and (model_list[0].empty() != model_list[1].empty()))
    {
        cerr << "models were specified only for strand " << (int)model_list[0].empty()
             << "! give models for both strands, or for neither." << endl;
        exit(EXIT_FAILURE);
    }
    if (not (model_list[0].empty() and model_list[1].empty() and model_list[2].empty()))
    {
        for (unsigned st = 0; st < 3; ++st)
        {
            for (const auto& e : model_list[st])
            {
                Pore_Model_Type pm;
                string pm_name = e;
                strict_fstream::ifstream ifs(e);
                ifs >> pm;
                pm.strand() = st;
                models[pm_name] = move(pm);
                LOG("main", info) << "loaded module [" << pm_name << "] for strand [" << st << "]" << endl;
            }
        }
    }
    else
    {
        // use built-in models
        for (unsigned i = 0; i < Builtin_Model::num; ++i)
        {
            Pore_Model_Type pm;
            string pm_name = Builtin_Model::names[i];
            pm.load_from_vector(Builtin_Model::init_lists[i]);
            pm.strand() = Builtin_Model::strands[i];
            models[Builtin_Model::names[i]] = move(pm);
            LOG("main", info) << "loaded builtin module [" << Builtin_Model::names[i] << "] for strand [" << Builtin_Model::strands[i] << "]" << endl;
        }
    }
}

void init_transitions(State_Transitions_Type& transitions)
{
    if (not opts::trans_fn.get().empty())
    {
        zstr::ifstream(opts::trans_fn) >> transitions;
        LOG("main", info) << "loaded state transitions from [" << opts::trans_fn.get() << "]" << endl;
    }
    else
    {
        transitions.compute_transitions(opts::pr_skip, opts::pr_stay, opts::pr_cutoff);
        LOG("main", info) << "initialized state transitions with parameters p_skip=[" << opts::pr_skip
                          << "], pr_stay=[" << opts::pr_stay << "], pr_cutoff=[" << opts::pr_cutoff << "]" << endl;
    }
}

void init_files(list< string >& files)
{}

void init_reads(list< string >& files, deque< Fast5_Summary_Type >& reads)
{}

void real_main()
{
    Model_Dict_Type models;
    State_Transitions_Type transitions;
    deque< Fast5_Summary_Type > reads;
    list< string > files;

    init_models(models);
    init_transitions(transitions);
    init_files(files);
    init_reads(files, reads);

    clog << "files:";
    for (auto& e : opts::input_fn)
    {
        clog << " " << e;
    }
    clog << endl;

    Fast5_Summary_Type fs(opts::input_fn.get().at(0));
    cerr << fs << endl;

    /*
    Pore_Model<> pm;
    State_Transitions<> st;
    vector< Event<> > ev;
    zstr::ifstream(opts::pm_file_name) >> pm;
    zstr::ifstream(opts::st_file_name) >> st;
    {
        zstr::ifstream ifs(opts::ev_file_name);
        Event<> e;
        while (ifs >> e)
        {
            ev.push_back(e);
        }
    }

    Viterbi<> vit;
    vit.fill(pm, st, ev);
    cout << vit.base_seq() << std::endl;
    */
}

int main(int argc, char * argv[])
{
    opts::cmd_parser.parse(argc, argv);
    Logger::set_levels_from_options(opts::log_level);
    real_main();
}

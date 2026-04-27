// PDNAM
// Copyright (C) 2026 Wasted Audio
//
// SPDX-License-Identifier: MIT

#include "m_pd.h"
#include "g_canvas.h"
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <filesystem>
#include "nlohmann/json.hpp"
#include "MicroNAM.h"

using json = nlohmann::json;

static t_class *pdnam_tilde_class;

struct Model {
    std::variant<
        std::monostate,
        MicroNAM::NanoNet<1>,
        MicroNAM::FeatherNet<1>,
        MicroNAM::LiteNet<1>,
        MicroNAM::StandardNet<1>
    > net;
    
    void load_weights(const std::vector<float>& weights) {
        const float* w = weights.data();
        if (weights.size() == 842) {
            net.emplace<MicroNAM::NanoNet<1>>();
            std::get<1>(net).load_weights(w);
        } else if (weights.size() == 3026) {
            net.emplace<MicroNAM::FeatherNet<1>>();
            std::get<2>(net).load_weights(w);
        } else if (weights.size() == 6554) {
            net.emplace<MicroNAM::LiteNet<1>>();
            std::get<3>(net).load_weights(w);
        } else if (weights.size() == 13802) {
            net.emplace<MicroNAM::StandardNet<1>>();
            std::get<4>(net).load_weights(w);
        }
    }

    void forward(const float* in, float* out) {
        std::visit([in, out](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (!std::is_same_v<T, std::monostate>) {
                arg.forward(in, out);
            } else {
                *out = *in;
            }
        }, net);
    }
    
    bool is_loaded() const {
        return !std::holds_alternative<std::monostate>(net);
    }
};

typedef struct _pdnam_tilde {
    t_object x_obj;
    t_sample f;
    std::unique_ptr<Model> model;
} t_pdnam_tilde;

static void pdnam_tilde_load(t_pdnam_tilde *x, t_symbol *s)
{
    char buf[MAXPDSTRING];
    canvas_makefilename(canvas_getcurrent(), s->s_name, buf, MAXPDSTRING);
    std::filesystem::path path(buf);

    if (!std::filesystem::exists(path)) {
        pd_error(x, "pdnam~: file not found: %s", buf);
        return;
    }

    try {
        std::ifstream f(path);
        json data = json::parse(f);

        if (data.contains("weights") && data["weights"].is_array()) {
            std::vector<float> weights = data["weights"].get<std::vector<float>>();
            auto new_model = std::make_unique<Model>();
            new_model->load_weights(weights);
            if (new_model->is_loaded()) {
                x->model = std::move(new_model);
                post("pdnam~: loaded model %s (%zu weights)", s->s_name, weights.size());
            } else {
                pd_error(x, "pdnam~: unsupported weight count: %zu", weights.size());
            }
        } else {
            pd_error(x, "pdnam~: JSON missing 'weights' array");
        }
    } catch (const std::exception& e) {
        pd_error(x, "pdnam~: error loading JSON: %s", e.what());
    }
}

static t_int *pdnam_tilde_perform(t_int *w)
{
    t_pdnam_tilde *x = (t_pdnam_tilde *)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);

    if (x->model && x->model->is_loaded()) {
        for (int i = 0; i < n; i++) {
            float in_val = (float)in[i];
            float out_val;
            x->model->forward(&in_val, &out_val);
            out[i] = (t_sample)out_val;
        }
    } else {
        for (int i = 0; i < n; i++) {
            out[i] = in[i];
        }
    }

    return (w + 5);
}

static void pdnam_tilde_dsp(t_pdnam_tilde *x, t_signal **sp)
{
    dsp_add(pdnam_tilde_perform, 4, x,
            sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

static void *pdnam_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_pdnam_tilde *x = (t_pdnam_tilde *)pd_new(pdnam_tilde_class);
    x->model = nullptr;

    if (argc >= 1 && argv[0].a_type == A_SYMBOL) {
        pdnam_tilde_load(x, atom_getsymbol(&argv[0]));
    }

    outlet_new(&x->x_obj, &s_signal);
    return (void *)x;
}

static void pdnam_tilde_free(t_pdnam_tilde *x)
{
    x->model.reset();
}

extern "C" void pdnam_tilde_setup(void) {
    pdnam_tilde_class = class_new(gensym("pdnam~"),
        (t_newmethod)pdnam_tilde_new,
        (t_method)pdnam_tilde_free, sizeof(t_pdnam_tilde),
        CLASS_DEFAULT, A_GIMME, 0);

    class_addmethod(pdnam_tilde_class,
        (t_method)pdnam_tilde_dsp, gensym("dsp"), A_CANT, 0);
    CLASS_MAINSIGNALIN(pdnam_tilde_class, t_pdnam_tilde, f);
}

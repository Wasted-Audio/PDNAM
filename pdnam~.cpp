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
#include <type_traits>
#include <optional>
#include "nlohmann/json.hpp"
#include "MicroNAM.h"

using json = nlohmann::json;

static t_class *pdnam_tilde_class;

// ---------------------------------------------------------------------------
// Model wrapper
// ---------------------------------------------------------------------------

struct Model {
    using Net = std::variant<
        std::monostate,
        MicroNAM::NanoNet<1>,
        MicroNAM::FeatherNet<1>,
        MicroNAM::LiteNet<1>,
        MicroNAM::StandardNet<1>,
        MicroNAM::A2LiteNet<1>,
        MicroNAM::A2FullNet<1>
    >;

    Net net;

    // weights.size() -> which net. Kept as the single source of truth so the
    // A1 (flat) and A2 (submodel) paths can't drift apart.
    bool load_weights(const std::vector<float>& weights) {
        switch (weights.size()) {
            case 842:   emplace<MicroNAM::NanoNet<1>>(weights);     return true;
            case 3026:  emplace<MicroNAM::FeatherNet<1>>(weights);  return true;
            case 6554:  emplace<MicroNAM::LiteNet<1>>(weights);     return true;
            case 13802: emplace<MicroNAM::StandardNet<1>>(weights); return true;
            case 1871:  emplace<MicroNAM::A2LiteNet<1>>(weights);   return true;
            case 12146: emplace<MicroNAM::A2FullNet<1>>(weights);   return true;
            default:    return false;
        }
    }

    void forward(const float* in, float* out) {
        std::visit([in, out](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                *out = *in;
            else
                arg.forward(in, out);
        }, net);
    }

    bool is_loaded() const {
        return !std::holds_alternative<std::monostate>(net);
    }

private:
    template <typename T>
    void emplace(const std::vector<float>& w) {
        net.template emplace<T>();
        std::get<T>(net).load_weights(w.data());
    }
};

// ---------------------------------------------------------------------------
// A2 submodel selection
// ---------------------------------------------------------------------------

enum class A2Size { Any, Lite, Full };

static bool a2_size_from_symbol(t_symbol *s, A2Size& out) {
    if (!s) return false;
    if (s == gensym("lite")) { out = A2Size::Lite; return true; }
    if (s == gensym("full")) { out = A2Size::Full; return true; }
    return false;
}

// A2-Full is 8 channels, A2-Lite is 3. Channels are in the submodel config,
// so we classify by structure rather than by weight count.
static int submodel_channels(const json& sub) {
    if (!sub.contains("model") || !sub["model"].is_object()) return -1;
    const json& m = sub["model"];
    if (!m.contains("config") || !m["config"].is_object()) return -1;
    const json& cfg = m["config"];
    if (cfg.contains("layers") && cfg["layers"].is_array() &&
        !cfg["layers"].empty() && cfg["layers"][0].contains("channels"))
        return cfg["layers"][0]["channels"].get<int>();
    return -1;
}

struct A2Choice {
    const json* sub = nullptr;
    int channels = -1;
    bool valid() const { return sub != nullptr; }
};

static A2Choice pick_a2_submodel(const json& data, A2Size want) {
    A2Choice best;
    if (!data.contains("config") || !data["config"].contains("submodels"))
        return best;
    const json& subs = data["config"]["submodels"];
    if (!subs.is_array()) return best;

    for (auto& sub : subs) {
        const int ch = submodel_channels(sub);
        const bool is_lite = (ch == 3);
        const bool is_full = (ch == 8);
        if (!is_lite && !is_full) continue;

        if (want == A2Size::Lite && !is_lite) continue;
        if (want == A2Size::Full && !is_full) continue;

        // Default (Any): keep the largest, which is the highest max_value
        // entry — Full for a full A2 export, Lite for a lite-only export.
        if (!best.valid() || ch > best.channels) {
            best.sub = &sub;
            best.channels = ch;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// The object
// ---------------------------------------------------------------------------

typedef struct _pdnam_tilde {
    t_object x_obj;
    t_sample f;
    t_canvas *canvas;
    Model *model;
    Model *retired;   // old model awaiting deletion, see swap_model()
} t_pdnam_tilde;

static void swap_model(t_pdnam_tilde *x, Model *fresh) {
    // The audio thread reads x->model once per perform call. Retiring the old
    // one instead of deleting it means no use-after-free even if a block is
    // in flight; it is freed in pdnam_tilde_free().
    Model *old = x->model;
    x->model = fresh;
    if (x->retired && x->retired != old) delete x->retired;
    x->retired = old;
}

static void pdnam_tilde_load(t_pdnam_tilde *x, t_symbol *s, t_symbol *want) {
    if (!s || s == &s_)
        return;

    A2Size want_size = A2Size::Any;
    if (want && want != &s_) {
        if (!a2_size_from_symbol(want, want_size)) {
            pd_error(x, "pdnam~: unknown model '%s' (use 'lite' or 'full')",
                     want->s_name);
            return;
        }
    }

    char buf[MAXPDSTRING];
    canvas_makefilename(x->canvas, s->s_name, buf, MAXPDSTRING);
    std::filesystem::path path(buf);

    if (!std::filesystem::exists(path)) {
        pd_error(x, "pdnam~: file not found: %s", buf);
        return;
    }

    try {
        std::ifstream f(path);
        if (!f) {
            pd_error(x, "pdnam~: cannot open %s", buf);
            return;
        }
        json data = json::parse(f);

        // --- A1: flat top-level weights ------------------------------------
        if (data.contains("weights") && data["weights"].is_array() &&
            !data["weights"].empty()) {
            if (want && want != &s_) {
                pd_error(x, "pdnam~: %s is an A1 model, it has no 'lite'/'full' variants",
                         s->s_name);
                return;
            }
            auto w = data["weights"].get<std::vector<float>>();
            auto m = std::make_unique<Model>();
            if (!m->load_weights(w)) {
                pd_error(x, "pdnam~: unsupported A1 model in %s (%zu weights)",
                         s->s_name, w.size());
                return;
            }
            swap_model(x, m.release());
            post("pdnam~: loaded A1 model %s (%zu weights)", s->s_name, w.size());
            return;
        }

        // --- A2: SlimmableContainer ----------------------------------------
        A2Choice choice = pick_a2_submodel(data, want_size);

        if (!choice.valid()) {
            if (want_size == A2Size::Lite || want_size == A2Size::Full) {
                pd_error(x, "pdnam~: %s has no '%s' submodel", s->s_name,
                         want->s_name);
            } else {
                pd_error(x, "pdnam~: no supported model found in %s", s->s_name);
            }
            return;
        }

        auto w = (*choice.sub)["model"]["weights"].get<std::vector<float>>();
        auto m = std::make_unique<Model>();
        if (!m->load_weights(w)) {
            pd_error(x, "pdnam~: unsupported A2 submodel in %s (%d ch, %zu weights)",
                     s->s_name, choice.channels, w.size());
            return;
        }
        swap_model(x, m.release());
        post("pdnam~: loaded A2-%s model %s (%d ch, %zu weights)",
             choice.channels == 3 ? "Lite" : "Full", s->s_name,
             choice.channels, w.size());

    } catch (const std::exception& e) {
        pd_error(x, "pdnam~: error loading JSON: %s", e.what());
    }
}

static t_int *pdnam_tilde_perform(t_int *w) {
    t_pdnam_tilde *x = (t_pdnam_tilde *)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);

    Model *model = x->model;   // single read, no tearing between samples

    if (model && model->is_loaded()) {
        for (int i = 0; i < n; i++) {
            float in_val = (float)in[i];
            float out_val = in_val;
            model->forward(&in_val, &out_val);
            out[i] = (t_sample)out_val;
        }
    } else {
        for (int i = 0; i < n; i++) out[i] = in[i];
    }

    return (w + 5);
}

static void pdnam_tilde_dsp(t_pdnam_tilde *x, t_signal **sp) {
    dsp_add(pdnam_tilde_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

static void *pdnam_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    t_pdnam_tilde *x = (t_pdnam_tilde *)pd_new(pdnam_tilde_class);
    x->canvas = canvas_getcurrent();
    x->model = nullptr;
    x->retired = nullptr;

    // pdnam~ <file> [lite|full]
    t_symbol *file = &s_;
    t_symbol *size = &s_;

    for (int i = 0; i < argc; i++) {
        if (argv[i].a_type != A_SYMBOL) {
            pd_error(x, "pdnam~: argument %d must be a symbol", i + 1);
            continue;
        }
        t_symbol *sym = atom_getsymbol(&argv[i]);
        if (i == 0) file = sym;
        else if (i == 1) size = sym;
        else pd_error(x, "pdnam~: ignoring extra argument '%s'", sym->s_name);
    }

    // A first argument of "lite"/"full" means the user forgot the file.
    if (file == gensym("lite") || file == gensym("full")) {
        pd_error(x, "pdnam~: expected pdnam~ <file> [lite|full], got '%s' first",
                 file->s_name);
        file = &s_;
    }

    if (file != &s_) pdnam_tilde_load(x, file, size);

    outlet_new(&x->x_obj, &s_signal);
    return (void *)x;
}

static void pdnam_tilde_free(t_pdnam_tilde *x) {
    delete x->model;
    x->model = nullptr;
    delete x->retired;
    x->retired = nullptr;
}

extern "C" void pdnam_tilde_setup(void) {
    pdnam_tilde_class = class_new(gensym("pdnam~"),
        (t_newmethod)pdnam_tilde_new,
        (t_method)pdnam_tilde_free, sizeof(t_pdnam_tilde),
        CLASS_DEFAULT, A_GIMME, 0);

    class_addmethod(pdnam_tilde_class,
        (t_method)pdnam_tilde_dsp, gensym("dsp"), A_CANT, 0);

    // Runtime switching: [load model.nam( or [load model.nam lite(
    class_addmethod(pdnam_tilde_class,
        (t_method)pdnam_tilde_load, gensym("load"),
        A_SYMBOL, A_DEFSYM, 0);

    CLASS_MAINSIGNALIN(pdnam_tilde_class, t_pdnam_tilde, f);
}

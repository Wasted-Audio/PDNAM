// PDNAM
// Copyright (C) 2026 Wasted Audio
//
// SPDX-License-Identifier: MIT

#include "m_pd.h"
#include "g_canvas.h"
#include "NeuralAudio/NeuralModel.h"
#include <iostream>
#include <string>
#include <filesystem>

static t_class *pdnam_tilde_class;

typedef struct _pdnam_tilde {
    t_object  x_obj;
    t_sample f;
    NeuralAudio::NeuralModel* model;
    t_symbol* model_path;
} t_pdnam_tilde;

static void pdnam_tilde_load(t_pdnam_tilde *x)
{
    if (x->model) {
        delete x->model;
        x->model = nullptr;
    }

    if (x->model_path == &s_) return;

    // Resolve relative path
    char buf[MAXPDSTRING];
    canvas_makefilename(canvas_getcurrent(), x->model_path->s_name, buf, MAXPDSTRING);
    std::filesystem::path path(buf);

    if (!std::filesystem::exists(path)) {
        pd_error(x, "pdnam~: model file not found: %s", buf);
        return;
    }

    // Set load mode
    NeuralAudio::EModelLoadMode mode = NeuralAudio::EModelLoadMode::Internal;

    NeuralAudio::NeuralModel::SetWaveNetLoadMode(mode);
    NeuralAudio::NeuralModel::SetLSTMLoadMode(mode);

    x->model = NeuralAudio::NeuralModel::CreateFromFile(path);
    if (x->model) {
        post("pdnam~: loaded model %s", x->model_path->s_name);
    } else {
        pd_error(x, "pdnam~: failed to load model %s", x->model_path->s_name);
    }
}

void *pdnam_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_pdnam_tilde *x = (t_pdnam_tilde *)pd_new(pdnam_tilde_class);

    x->model = nullptr;
    x->model_path = &s_;

    if (argc >= 1 && argv[0].a_type == A_SYMBOL) {
        x->model_path = atom_getsymbol(&argv[0]);
    }

    pdnam_tilde_load(x);

    outlet_new(&x->x_obj, &s_signal);

    return (void *)x;
}

t_int *pdnam_tilde_perform(t_int *w)
{
    t_pdnam_tilde *x = (t_pdnam_tilde *)(w[1]);
    t_sample  *in = (t_sample *)(w[2]);
    t_sample  *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);

    if (x->model) {
        x->model->Process(in, out, n);
    } else {
        while (n--) {
            *out++ = *in++;
        }
    }

    return (w+5);
}

void pdnam_tilde_dsp(t_pdnam_tilde *x, t_signal **sp)
{
    dsp_add(pdnam_tilde_perform, 4, x,
            sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

void pdnam_tilde_free(t_pdnam_tilde *x)
{
    if (x->model) delete x->model;
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

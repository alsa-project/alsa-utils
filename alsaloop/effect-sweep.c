/*
 *  Bandpass filter sweep effect
 *  Copyright (c) Maarten de Boer <mdeboer@iua.upf.es>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <math.h>
#include <alsa/asoundlib.h>

struct effect_private {
	/* filter the sweep variables */
	float lfo,dlfo,fs,fc,BW,C,D,a0,a1,a2,b1,b2,*x[3],*y[3];
	float lfo_depth, lfo_center;
	unsigned int channels;
};

static int effect_init(struct lookback *loopback,
		       void *private_data,
		       snd_pcm_access_t access,
		       unsigned int channels,
		       unsigned int rate,
		       snd_pcm_format_t format)
{
	struct effect_private *priv = private_data;
	int i;

#if __BYTE_ORDER == __LITTLE_ENDIAN
	if (format != SND_PCM_FORMAT_S16_LE)
		return -EIO;
#elif __BYTE_ORDER == __BIG_ENDIAN
	if (format != SND_PCM_FORMAT_S16_BE)
		return -EIO;
#else
	return -EIO;
#endif
	priv->fs = (float) rate;
	priv->channels = channels;
	for (i = 0; i < 3; i++) {
		priv->x[i] = calloc(channels * sizeof(float));
		priv->y[i] = calloc(channels * sizeof(float));
	}
	return 0;
}

static int effect_done(struct loopback *loopback,
		       void *private_data)
{
	struct effect_private *priv = private_data;
	int i;

	for (i = 0; i < 3; i++) {
		free(priv->x[i]);
		free(priv->y[i]);
	}
	return 0;
}

static int effect_apply(struct loopback *loopback,
			void *private_data,
			const snd_pcm_channel_area_t *areas,
			snd_uframes_t offset,
			snd_uframes_t frames)
{
	struct effect_private *priv = private_data;
	short *samples = (short*)areas[0].addr + offset*priv->channels;
	snd_uframes_t i;

	for (i=0; i < frames; i++) {
		int chn;

		fc = sin(priv->lfo)*priv->lfo_depth+priv->lfo_center;
		priv->lfo += priv->dlfo;
		if (priv->lfo>2.*M_PI) priv->lfo -= 2.*M_PI;
		priv->C = 1./tan(M_PI*priv->BW/priv->fs);
		priv->D = 2.*cos(2*M_PI*fc/fs);
		priv->a0 = 1./(1.+priv->C);
		priv->a1 = 0;
		priv->a2 = -priv->a0;
		priv->b1 = -priv->C*priv->D*a0;
		priv->b2 = (priv->C-1)*priv->a0;

		for (chn=0; chn < priv->channels; chn++)
		{
			priv->x[chn][2] = priv->x[chn][1];
			priv->x[chn][1] = priv->x[chn][0];

			priv->y[chn][2] = priv->y[chn][1];
			priv->y[chn][1] = priv->y[chn][0];

			priv->x[chn][0] = samples[i*channels+chn];
			priv->y[chn][0] = priv->a0*priv->x[0][chn]
				+ priv->a1*priv->x[1][chn] + priv->a2*x[2][chn]
				- priv->b1*priv->y[1][chn] - priv->b2*y[2][chn];
			samples[i*channels+chn] = priv->y[chn][0];
		}
	}
	return 0;
}

void effect_init_sweep(void)
{
	struct effect_private *priv;

	priv = register_effect(effect_init,
			       effect_apply,
			       effect_done,
			       sizeof(struct effectprivate));
	if (priv) {
		priv->lfo_center = 2000.;
		priv->lfo_depth = 1800.;
		priv->lfo_freq = 0.2;
		priv->BW = 50;
	}
}

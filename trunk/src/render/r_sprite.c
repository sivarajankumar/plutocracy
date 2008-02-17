/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Implements 2D sprites and text rendering with the SDL TTF library. This
   should be the only file that calls functions from the library. */

#include "r_common.h"

static struct {
        TTF_Font *ttf_font;
        float scale;
} fonts[R_FONTS];

/******************************************************************************\
 Initialize a sprite structure.
\******************************************************************************/
void R_sprite_init(r_sprite_t *sprite, const char *filename)
{
        if (!sprite)
                return;
        C_zero(sprite);
        sprite->red = 1.f;
        sprite->green = 1.f;
        sprite->blue = 1.f;
        sprite->alpha = 1.f;
        if (!filename || !filename[0])
                return;
        sprite->texture = R_texture_load(filename, FALSE);
        if (sprite->texture) {
                sprite->size.x = sprite->texture->surface->w;
                sprite->size.y = sprite->texture->surface->h;
        }
}

/******************************************************************************\
 Initialize a sprite structure.
\******************************************************************************/
void R_sprite_cleanup(r_sprite_t *sprite)
{
        if (!sprite)
                return;
        R_texture_free(sprite->texture);
        C_zero(sprite);
}

/******************************************************************************\
 Renders a 2D textured quad on screen. If we are rendering a rotated sprite
 that doesn't use alpha, this function will draw an anti-aliased border for it.
 The coordinates work here like you would expect for 2D, the origin is in the
 upper left of the sprite and y decreases down the screen.
   TODO: Disable anti-aliasing here if FSAA is on.
   FIXME: There is a slight overlap between the antialiased edge line and the
          polygon which should probably be fixed if possible.
\******************************************************************************/
void R_sprite_render(const r_sprite_t *sprite)
{
        r_vertex2_t verts[4];
        unsigned short indices[] = { 0, 1, 2, 3, 0 };
        float dw, dh;

        if (!sprite || !sprite->texture)
                return;
        R_set_mode(R_MODE_2D);
        R_texture_select(sprite->texture);

        /* Modulate color */
        glColor4f(sprite->red, sprite->green, sprite->blue, sprite->alpha);
        if (sprite->alpha < 1.f)
                glEnable(GL_BLEND);

        dw = sprite->size.x / 2;
        dh = sprite->size.y / 2;
        verts[0].co = C_vec3(-dw, dh, 0.f);
        verts[0].uv = C_vec2(0.f, 1.f);
        verts[1].co = C_vec3(-dw, -dh, 0.f);
        verts[1].uv = C_vec2(0.f, 0.f);
        verts[2].co = C_vec3(dw, -dh, 0.f);
        verts[2].uv = C_vec2(1.f, 0.f);
        verts[3].co = C_vec3(dw, dh, 0.f);
        verts[3].uv = C_vec2(1.f, 1.f);
        C_count_add(&r_count_faces, 2);
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(sprite->origin.x + dw, sprite->origin.y + dh, 0.f);
        glRotatef(C_rad_to_deg(sprite->angle), 0.0, 0.0, 1.0);
        glInterleavedArrays(R_VERTEX2_FORMAT, 0, verts);
        glDrawElements(GL_QUADS, 4, GL_UNSIGNED_SHORT, indices);

        /* Draw the edge lines to anti-alias non-alpha quads */
        if (!sprite->texture->alpha && sprite->angle != 0.f &&
            sprite->alpha == 1.f) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_BLEND);
                glDrawElements(GL_LINE_STRIP, 5, GL_UNSIGNED_SHORT, indices);
        }

        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
        glPopMatrix();
        R_check_errors();
}

/******************************************************************************\
 Loads the font, properly scaled and generates an error message if something
 goes wrong.
\******************************************************************************/
static void load_font(r_font_t font, const char *path, int size)
{
        int points;

        points = ceilf(size * r_pixel_scale.value.f);
        fonts[font].scale = (float)size / points;
        if (points < R_FONT_SIZE_MIN)
                points = R_FONT_SIZE_MIN;
        fonts[font].ttf_font = TTF_OpenFont(path, points);
        if (!fonts[font].ttf_font)
                C_error("Failed to load font '%s' (%d)", path, size);
}

/******************************************************************************\
 Initialize the SDL TTF library and generate font structures.
\******************************************************************************/
void R_init_fonts(void)
{
        SDL_version compiled;
        const SDL_version *linked;
        int i;

        TTF_VERSION(&compiled);
        C_debug("Compiled with SDL_ttf %d.%d.%d",
                compiled.major, compiled.minor, compiled.patch);
        linked = TTF_Linked_Version();
        if (!linked)
                C_error("Failed to get SDL_ttf linked version");
        C_debug("Linked with SDL_ttf %d.%d.%d",
                linked->major, linked->minor, linked->patch);
        if (TTF_Init() == -1)
                C_error("Failed to initialize SDL TTF library: %s",
                        TTF_GetError());

        /* Load font files */
        load_font(R_FONT_CONSOLE, "gui/fonts/VeraMoBd.ttf", 12);
        for (i = 0; i < R_FONTS; i++)
                if (!fonts[i].ttf_font)
                        C_error("Forgot to load font %d", i);
}

/******************************************************************************\
 Cleanup the SDL TTF library.
\******************************************************************************/
void R_cleanup_fonts(void)
{
        int i;

        for (i = 0; i < R_FONTS; i++)
                TTF_CloseFont(fonts[i].ttf_font);
        TTF_Quit();
}

/******************************************************************************\
 Blits the entire [src] surface to dest at [x], [y] and applies a one-pixel
 shadow of [alpha] opacity. This function could be made more efficient by
 unrolling the loops a bit, but it is not worth the loss in readablitiy.
\******************************************************************************/
static void blit_shadowed(SDL_Surface *dest, SDL_Surface *src,
                          int to_x, int to_y, float alpha)
{
        int y, x;

        if (SDL_LockSurface(dest) < 0) {
                C_warning("Failed to lock destination surface");
                return;
        }
        if (SDL_LockSurface(src) < 0) {
                C_warning("Failed to lock source surface");
                return;
        }
        for (y = 0; y < src->h + 1; y++)
                for (x = 0; x < src->w + 1; x++) {
                        c_color_t dc, sc;

                        dc = R_SDL_get_pixel(dest, to_x + x, to_y + y);
                        if (x < src->w && y < src->h) {
                                sc = R_SDL_get_pixel(src, x, y);
                                dc = C_color_blend(dc, sc);
                        }
                        if (dc.a < 1.f && x && y && alpha) {
                                sc = R_SDL_get_pixel(src, x - 1, y - 1);
                                sc = C_color(0, 0, 0, sc.a * alpha);
                                dc = C_color_blend(sc, dc);
                        }
                        R_SDL_put_pixel(dest, to_x + x, to_y + y, dc);
                }
        SDL_UnlockSurface(dest);
        SDL_UnlockSurface(src);
}

/******************************************************************************\
 If the text or font has changed, this function will clean up old resources
 and generate a new SDL surface and texture. This function will setup the
 sprite member of the text structure for rendering. Note that sprite size will
 be reset if it is regenerated. Text can be wrapped (or not, set wrap to 0)
 and a shadow (of variable opacity) can be applied. [string] does need to
 persist after the function call.
\******************************************************************************/
void R_text_set_text(r_text_t *text, r_font_t font, float wrap, float shadow,
                     const char *string)
{
        r_texture_t *tex;
        int i, j, y, last_break, last_line, width, height, line_skip;
        char buf[1024], *line;

        if (font < 0 || font >= R_FONTS)
                C_error("Invalid font index %d", font);
        if (!text || (font == text->font && !strcmp(string, text->string)))
                return;
        if (!string || !string[0]) {
                text->string[0] = NUL;
                R_sprite_cleanup(&text->sprite);
                return;
        }
        C_strncpy_buf(text->string, string);
        R_sprite_cleanup(&text->sprite);

        /* Wrap the text and figure out how large the surface needs to be.
           For some reason, SDL_ttf returns a line skip that is one pixel shy
           of the image height returned by TTF_RenderUTF8_Blended(). The
           width and height also need to be expanded by a pixel to leave room
           for the shadow. */
        wrap /= fonts[font].scale;
        last_line = 0;
        last_break = 0;
        width = 0;
        height = (line_skip = TTF_FontLineSkip(fonts[font].ttf_font)) + 2;
        line = buf;
        for (i = 0, j = 0; string[i]; i++) {
                int w, h;

                if (j >= sizeof (buf) - 1) {
                        C_warning("Ran out of buffer space");
                        break;
                }
                buf[j++] = string[i];
                buf[j] = NUL;
                if (C_is_space(string[i]))
                        last_break = i;
                TTF_SizeText(fonts[font].ttf_font, line, &w, &h);
                if ((wrap > 0.f && w > wrap) || string[i] == '\n') {
                        if (last_line == last_break)
                                last_break = last_line >= i - 1 ? i : i - 1;
                        j -= i - last_break + 1;
                        buf[j++] = '\n';
                        line = buf + j;
                        while (C_is_space(string[last_break]))
                                last_break++;
                        i = last_break - 1;
                        last_line = last_break;
                        height += line_skip;
                        continue;
                }
                if (w > width)
                        width = w;
        }
        if (++width < 2 || height < 2)
                return;

        /* SDL_ttf will generate a 32-bit RGBA surface with the text printed
           on it in white (we can modulate the color later) but it won't
           recognize newlines so we need to print each line to a temporary
           surface and blit to the final surface (in our format). */
        tex = R_texture_alloc(width, height, TRUE);
        for (i = 0, last_break = 0, y = 0; ; i++) {
                SDL_Color white = { 255, 255, 255, 255 };
                SDL_Surface *surf;
                char swap;

                if (buf[i] != '\n' && buf[i])
                        continue;
                swap = buf[i];
                buf[i] = NUL;
                surf = TTF_RenderUTF8_Blended(fonts[font].ttf_font,
                                              buf + last_break, white);
                if (!surf) {
                        C_warning("TTF_RenderUTF8_Blended() failed: %s",
                                  TTF_GetError());
                        break;
                }
                blit_shadowed(tex->surface, surf, 0, y, shadow);
                SDL_FreeSurface(surf);
                if (!swap)
                        break;
                last_break = i + 1;
                y += line_skip;
        }
        R_texture_upload(tex, FALSE);

        /* Text is actually just a sprite and after this function has finished
           the sprite itself can be manipulated as expected */
        R_sprite_init(&text->sprite, NULL);
        text->sprite.texture = tex;
        text->sprite.size.x = width * fonts[font].scale;
        text->sprite.size.y = height * fonts[font].scale;
        text->font = font;
}

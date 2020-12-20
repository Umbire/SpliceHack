/* NetHack 3.7	o_init.c	$NHDT-Date: 1596498193 2020/08/03 23:43:13 $  $NHDT-Branch: NetHack-3.7 $:$NHDT-Revision: 1.43 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2011. */
/* NetHack may be freely redistributed.  See license for details. */

/* Edited on 3/11/18 by NullCGT */

#include "hack.h"

static void FDECL(setgemprobs, (d_level *));
static void FDECL(shuffle, (int, int, BOOLEAN_P));
static void NDECL(shuffle_all);
static boolean FDECL(interesting_to_discover, (int));
static int FDECL(CFDECLSPEC discovered_cmp, (const genericptr,
                                             const genericptr));
static char *FDECL(oclass_to_name, (CHAR_P, char *));

#ifdef USE_TILES
static void NDECL(shuffle_tiles);
extern short glyph2tile[]; /* from tile.c */

/* Shuffle tile assignments to match descriptions, so a red potion isn't
 * displayed with a blue tile and so on.
 *
 * Tile assignments are not saved, and shouldn't be so that a game can
 * be resumed on an otherwise identical non-tile-using binary, so we have
 * to reshuffle the assignments from oc_descr_idx information when a game
 * is restored.  So might as well do that the first time instead of writing
 * another routine.
 */
static void
shuffle_tiles()
{
    int i;
    short tmp_tilemap[NUM_OBJECTS];

    for (i = 0; i < NUM_OBJECTS; i++)
        tmp_tilemap[i] = glyph2tile[objects[i].oc_descr_idx + GLYPH_OBJ_OFF];

    for (i = 0; i < NUM_OBJECTS; i++)
        glyph2tile[i + GLYPH_OBJ_OFF] = tmp_tilemap[i];
}
#endif /* USE_TILES */

static void
setgemprobs(dlev)
d_level *dlev;
{
    int j, first, lev;

    if (dlev)
        lev = (ledger_no(dlev) > maxledgerno()) ? maxledgerno()
                                                : ledger_no(dlev);
    else
        lev = 0;
    first = g.bases[GEM_CLASS];

    for (j = 0; j < 9 - lev / 3; j++)
        objects[first + j].oc_prob = 0;
    first += j;
    if (first > LAST_GEM || objects[first].oc_class != GEM_CLASS
        || OBJ_NAME(objects[first]) == (char *) 0) {
        raw_printf("Not enough gems? - first=%d j=%d LAST_GEM=%d", first, j,
                   LAST_GEM);
        wait_synch();
    }
    for (j = first; j <= LAST_GEM; j++)
        objects[j].oc_prob = (171 + j - first) / (LAST_GEM + 1 - first);
}

/* shuffle descriptions on objects o_low to o_high */
static void
shuffle(o_low, o_high, domaterial)
int o_low, o_high;
boolean domaterial;
{
    int i, j, num_to_shuffle;
    short sw;
    int color;

    for (num_to_shuffle = 0, j = o_low; j <= o_high; j++)
        if (!objects[j].oc_name_known)
            num_to_shuffle++;
    if (num_to_shuffle < 2)
        return;

    for (j = o_low; j <= o_high; j++) {
        if (objects[j].oc_name_known)
            continue;
        do
            i = j + rn2(o_high - j + 1);
        while (objects[i].oc_name_known);
        sw = objects[j].oc_descr_idx;
        objects[j].oc_descr_idx = objects[i].oc_descr_idx;
        objects[i].oc_descr_idx = sw;
        sw = objects[j].oc_tough;
        objects[j].oc_tough = objects[i].oc_tough;
        objects[i].oc_tough = sw;
        color = objects[j].oc_color;
        objects[j].oc_color = objects[i].oc_color;
        objects[i].oc_color = color;

        /* shuffle material */
        if (domaterial) {
            sw = objects[j].oc_material;
            objects[j].oc_material = objects[i].oc_material;
            objects[i].oc_material = sw;
        }
    }
}

void
init_objects()
{
    int i, first, last, prevoclass;
    char oclass;
#ifdef TEXTCOLOR
#define COPY_OBJ_DESCR(o_dst, o_src) \
    o_dst.oc_descr_idx = o_src.oc_descr_idx, o_dst.oc_color = o_src.oc_color
#else
#define COPY_OBJ_DESCR(o_dst, o_src) o_dst.oc_descr_idx = o_src.oc_descr_idx
#endif

    /* bug fix to prevent "initialization error" abort on Intel Xenix.
     * reported by mikew@semike
     */
    for (i = 0; i <= MAXOCLASSES; i++)
        g.bases[i] = 0;
    /* initialize object descriptions */
    for (i = 0; i < NUM_OBJECTS; i++)
        objects[i].oc_name_idx = objects[i].oc_descr_idx = i;
    /* init base; if probs given check that they add up to 1000,
       otherwise compute probs */
    first = 0;
    prevoclass = -1;
    while (first < NUM_OBJECTS) {
        oclass = objects[first].oc_class;
        /*
         * objects[] sanity check:  must be in ascending oc_class order to
         * be able to use bases[class+1]-1 for the end of a class's range.
         * Also catches a non-contiguous class because reverting to any
         * earlier class would involve switching back to a lower class
         * number after having moved on to one or more other classes.
         */
        if ((int) oclass < prevoclass)
            panic("objects[%d] class #%d not in order!", first, oclass);

        last = first + 1;
        while (last < NUM_OBJECTS && objects[last].oc_class == oclass)
            last++;
        g.bases[(int) oclass] = first;

        if (oclass == GEM_CLASS) {
            setgemprobs((d_level *) 0);

            if (rn2(2)) { /* change turquoise from green to blue? */
                COPY_OBJ_DESCR(objects[TURQUOISE], objects[SAPPHIRE]);
            }
            if (rn2(2)) { /* change aquamarine from green to blue? */
                COPY_OBJ_DESCR(objects[AQUAMARINE], objects[SAPPHIRE]);
            }
            switch (rn2(4)) { /* change fluorite from violet? */
            case 0:
                break;
            case 1: /* blue */
                COPY_OBJ_DESCR(objects[FLUORITE], objects[SAPPHIRE]);
                break;
            case 2: /* white */
                COPY_OBJ_DESCR(objects[FLUORITE], objects[DIAMOND]);
                break;
            case 3: /* green */
                COPY_OBJ_DESCR(objects[FLUORITE], objects[EMERALD]);
                break;
            }
        }
        first = last;
        prevoclass = (int) oclass;
    }
    /* extra entry allows deriving the range of a class via
       bases[class] through bases[class+1]-1 for all classes */
    g.bases[MAXOCLASSES] = NUM_OBJECTS;
    /* hypothetically someone might remove all objects of some class,
       or be adding a new class and not populated it yet, leaving gaps
       in bases[]; guarantee that there are no such gaps */
    for (last = MAXOCLASSES - 1; last >= 0; --last)
        if (!g.bases[last])
            g.bases[last] = g.bases[last + 1];

    /* shuffle descriptions */
    shuffle_all();
#ifdef USE_TILES
    shuffle_tiles();
#endif
    objects[WAN_NOTHING].oc_dir = rn2(2) ? NODIR : IMMEDIATE;
}

/* retrieve the range of objects that otyp shares descriptions with */
void
obj_shuffle_range(otyp, lo_p, hi_p)
int otyp;         /* input: representative item */
int *lo_p, *hi_p; /* output: range that item belongs among */
{
    int i, ocls = objects[otyp].oc_class;

    /* default is just the object itself */
    *lo_p = *hi_p = otyp;

    switch (ocls) {
    case ARMOR_CLASS:
        if (otyp >= HELMET && otyp <= HELM_OF_TELEPATHY)
            *lo_p = HELMET, *hi_p = HELM_OF_TELEPATHY;
        else if (otyp >= GLOVES && otyp <= GAUNTLETS_OF_DEXTERITY)
            *lo_p = GLOVES, *hi_p = GAUNTLETS_OF_DEXTERITY;
        else if (otyp >= CLOAK_OF_PROTECTION && otyp <= CLOAK_OF_DISPLACEMENT)
            *lo_p = CLOAK_OF_PROTECTION, *hi_p = CLOAK_OF_DISPLACEMENT;
        else if (otyp >= MYSTIC_ROBE && otyp <= ROBE_OF_STASIS)
            *lo_p = MYSTIC_ROBE, *hi_p = ROBE_OF_STASIS;
        else if (otyp >= SPEED_BOOTS && otyp <= LEVITATION_BOOTS)
            *lo_p = SPEED_BOOTS, *hi_p = LEVITATION_BOOTS;
        break;
    case POTION_CLASS:
        /* potion of water has the only fixed description */
        *lo_p = g.bases[POTION_CLASS];
        *hi_p = POT_WATER - 1;
        break;
    case AMULET_CLASS:
    case SCROLL_CLASS:
    case SPBOOK_CLASS:
        /* exclude non-magic types and also unique ones */
        *lo_p = g.bases[ocls];
        for (i = *lo_p; objects[i].oc_class == ocls; i++)
            if (objects[i].oc_unique || !objects[i].oc_magic)
                break;
        *hi_p = i - 1;
        break;
    case RING_CLASS:
    case WAND_CLASS:
        /* entire class */
        *lo_p = g.bases[ocls];
        *hi_p = g.bases[ocls + 1] - 1;
        break;
    case TOOL_CLASS:
        if (otyp >= SPIRIT_CANDLE && otyp <= CALLING_CANDLE)
            *lo_p = SPIRIT_CANDLE, *hi_p = CALLING_CANDLE;
        break;
    }

    /* artifact checking might ask about item which isn't part of any range
       but fell within the classes that do have ranges specified above */
    if (otyp < *lo_p || otyp > *hi_p)
        *lo_p = *hi_p = otyp;
    return;
}

/* randomize object descriptions */
static void
shuffle_all()
{
    /* entire classes; obj_shuffle_range() handles their exceptions */
    static char shuffle_classes[] = {
        AMULET_CLASS, POTION_CLASS, RING_CLASS,  SCROLL_CLASS,
        SPBOOK_CLASS, WAND_CLASS
    };
    /* sub-class type ranges (one item from each group) */
    static short shuffle_types[] = {
        HELMET, GLOVES, CLOAK_OF_PROTECTION, MYSTIC_ROBE, SPEED_BOOTS, VENOM_CLASS,
        SPIRIT_CANDLE,
    };
    int first, last, idx;

    /* do whole classes (amulets, &c) */
    for (idx = 0; idx < SIZE(shuffle_classes); idx++) {
        obj_shuffle_range(g.bases[(int) shuffle_classes[idx]], &first, &last);
        shuffle(first, last, TRUE);
    }
    /* do type ranges (helms, &c) */
    for (idx = 0; idx < SIZE(shuffle_types); idx++) {
        obj_shuffle_range(shuffle_types[idx], &first, &last);
        shuffle(first, last, FALSE);
    }
    return;
}

/* Return TRUE if the provided string matches the unidentified description of
 * the provided object. */
boolean
objdescr_is(obj, descr)
struct obj *obj;
const char *descr;
{
    const char *objdescr;

    if (!obj) {
        impossible("objdescr_is: null obj");
        return FALSE;
    }

    objdescr = OBJ_DESCR(objects[obj->otyp]);
    if (!objdescr)
        return FALSE; /* no obj description, no match */
    return !strcmp(objdescr, descr);
}

/* find the object index for snow boots; used [once] by slippery ice code */
int
find_skates()
{
    register int i;
    register const char *s;

    for (i = SPEED_BOOTS; i <= LEVITATION_BOOTS; i++)
        if ((s = OBJ_DESCR(objects[i])) != 0 && !strcmp(s, "snow boots"))
            return i;

    impossible("snow boots not found?");
    return -1; /* not 0, or caller would try again each move */
}

/* level dependent initialization */
void
oinit()
{
    setgemprobs(&u.uz);
}

void
savenames(nhfp)
NHFILE *nhfp;
{
    int i;
    unsigned int len;

    if (perform_bwrite(nhfp)) {
        if (nhfp->structlevel) {
            bwrite(nhfp->fd, (genericptr_t)g.bases, sizeof g.bases);
            bwrite(nhfp->fd, (genericptr_t)g.disco, sizeof g.disco);
            bwrite(nhfp->fd, (genericptr_t)objects,
                   sizeof(struct objclass) * NUM_OBJECTS);
        }
    }
    /* as long as we use only one version of Hack we
       need not save oc_name and oc_descr, but we must save
       oc_uname for all objects */
    for (i = 0; i < NUM_OBJECTS; i++)
        if (objects[i].oc_uname) {
            if (perform_bwrite(nhfp)) {
                len = strlen(objects[i].oc_uname) + 1;
                if (nhfp->structlevel) {
                    bwrite(nhfp->fd, (genericptr_t)&len, sizeof len);
                    bwrite(nhfp->fd, (genericptr_t)objects[i].oc_uname, len);
                }
            }
            if (release_data(nhfp)) {
                free((genericptr_t) objects[i].oc_uname);
                objects[i].oc_uname = 0;
            }
        }
}

void
restnames(nhfp)
NHFILE *nhfp;
{
    int i;
    unsigned int len = 0;

    if (nhfp->structlevel) {
        mread(nhfp->fd, (genericptr_t) g.bases, sizeof g.bases);
        mread(nhfp->fd, (genericptr_t) g.disco, sizeof g.disco);
        mread(nhfp->fd, (genericptr_t) objects,
                sizeof(struct objclass) * NUM_OBJECTS);
    }
    for (i = 0; i < NUM_OBJECTS; i++) {
        if (objects[i].oc_uname) {
            if (nhfp->structlevel) {
                mread(nhfp->fd, (genericptr_t) &len, sizeof len);
            }
            objects[i].oc_uname = (char *) alloc(len);
            if (nhfp->structlevel) {
                mread(nhfp->fd, (genericptr_t)objects[i].oc_uname, len);
            }
	}
    }
#ifdef USE_TILES
    shuffle_tiles();
#endif
}

void
discover_object(oindx, mark_as_known, credit_hero)
register int oindx;
boolean mark_as_known;
boolean credit_hero;
{
    if (!objects[oindx].oc_name_known) {
        register int dindx, acls = objects[oindx].oc_class;

        /* Loop thru disco[] 'til we find the target (which may have been
           uname'd) or the next open slot; one or the other will be found
           before we reach the next class...
         */
        for (dindx = g.bases[acls]; g.disco[dindx] != 0; dindx++)
            if (g.disco[dindx] == oindx)
                break;
        g.disco[dindx] = oindx;

        if (mark_as_known) {
            objects[oindx].oc_name_known = 1;
            if (credit_hero)
                exercise(A_WIS, TRUE);
        }
        /* moves==1L => initial inventory, gameover => final disclosure */
        if (g.moves > 1L && !g.program_state.gameover) {
            if (objects[oindx].oc_class == GEM_CLASS)
                gem_learned(oindx); /* could affect price of unpaid gems */
            update_inventory();
        }
    }
}

/* if a class name has been cleared, we may need to purge it from disco[] */
void
undiscover_object(oindx)
register int oindx;
{
    if (!objects[oindx].oc_name_known) {
        register int dindx, acls = objects[oindx].oc_class;
        register boolean found = FALSE;

        /* find the object; shift those behind it forward one slot */
        for (dindx = g.bases[acls]; dindx < NUM_OBJECTS && g.disco[dindx] != 0
                                  && objects[dindx].oc_class == acls;
             dindx++)
            if (found)
                g.disco[dindx - 1] = g.disco[dindx];
            else if (g.disco[dindx] == oindx)
                found = TRUE;

        /* clear last slot */
        if (found)
            g.disco[dindx - 1] = 0;
        else
            impossible("named object not in disco");
        if (objects[oindx].oc_class == GEM_CLASS)
            gem_learned(oindx); /* ok, it's actually been unlearned */
        update_inventory();
    }
}

static boolean
interesting_to_discover(i)
register int i;
{
    /* Pre-discovered objects are now printed with a '*' */
    return (boolean) (objects[i].oc_uname != (char *) 0
                      || (objects[i].oc_name_known
                          && OBJ_DESCR(objects[i]) != (char *) 0));
}

/* items that should stand out once they're known */
static const short uniq_objs[] = {
    AMULET_OF_YENDOR, SPE_BOOK_OF_THE_DEAD, CANDELABRUM_OF_INVOCATION,
    BELL_OF_OPENING,
};

/* discoveries qsort comparison function */
static int CFDECLSPEC
discovered_cmp(v1, v2)
const genericptr v1;
const genericptr v2;
{
    const char *s1 = *(const char **) v1;
    const char *s2 = *(const char **) v2;
    /* each element starts with "* " or "  " but we don't sort by those */
    int res = strcmpi(s1 + 2, s2 + 2);

    if (res == 0) {
        ; /* no tie-breaker needed */
    }
    return res;
}

static char *
sortloot_descr(otyp, outbuf)
int otyp;
char *outbuf;
{
    Loot sl_cookie;
    struct obj o;

    o = cg.zeroobj;
    o.otyp = otyp;
    o.oclass = objects[otyp].oc_class;
    o.dknown = 1;
    o.known = (objects[otyp].oc_name_known || !objects[otyp].oc_uses_known)
              ? 1 : 0;
    o.corpsenm = NON_PM; /* suppress statue and figurine details */
    /* but suppressing fruit details leads to "bad fruit #0" */
    if (otyp == SLIME_MOLD)
        o.spe = g.context.current_fruit;

    (void) memset((genericptr_t) &sl_cookie, 0, sizeof sl_cookie);
    sl_cookie.obj = (struct obj *) 0;
    sl_cookie.str = (char *) 0;

    loot_classify(&sl_cookie, &o);
    Sprintf(outbuf, "%02d%02d%1d ",
            sl_cookie.orderclass, sl_cookie.subclass, sl_cookie.disco);
    return outbuf;
}

#define DISCO_BYCLASS      0 /* by discovery order within each class */
#define DISCO_SORTLOOT     1 /* by discovery order within each subclass */
#define DISCO_ALPHABYCLASS 2 /* alphabetized within each class */
#define DISCO_ALPHABETIZED 3 /* alphabetized across all classes */
/* also used in options.c (optfn_sortdiscoveries) */
const char disco_order_let[] = "osca";
const char *const disco_orders_descr[] = {
    "by order of discovery within each class",
    "sortloot order (by class with some sub-class groupings)",
    "alphabetical within each class",
    "alphabetical across all classes",
    (char *) 0
};

int
choose_disco_sort(mode)
int mode; /* 0 => 'O' cmd, 1 => full discoveries; 2 => class discoveries */
{
    winid tmpwin;
    menu_item *selected;
    anything any;
    int i, n, choice;

    tmpwin = create_nhwindow(NHW_MENU);
    start_menu(tmpwin, MENU_BEHAVE_STANDARD);
    any = cg.zeroany; /* zero out all bits */
    for (i = 0; disco_orders_descr[i]; ++i) {
        any.a_int = disco_order_let[i];
        add_menu(tmpwin, NO_GLYPH, &any, (char) any.a_int, 0, ATR_NONE,
                 disco_orders_descr[i],
                 (disco_order_let[i] == flags.discosort)
                    ? MENU_ITEMFLAGS_SELECTED
                    : MENU_ITEMFLAGS_NONE);
    }
    if (mode == 2) {
        /* called via 'm `' where full alphabetize doesn't make sense
           (only showing one class so can't span all classes) but the
           chosen sort will stick and also apply to '\' usage */
        any = cg.zeroany;
        add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE,
                 "", MENU_ITEMFLAGS_NONE);
        add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE,
                 "Note: full alphabetical and alphabetical within class",
                 MENU_ITEMFLAGS_NONE);
        add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE,
                 "      are equivalent for single class discovery, but",
                 MENU_ITEMFLAGS_NONE);
        add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE,
                 "      will matter for future use of total discoveries.",
                 MENU_ITEMFLAGS_NONE);
    }
    end_menu(tmpwin, "Ordering of discoveries");

    n = select_menu(tmpwin, PICK_ONE, &selected);
    destroy_nhwindow(tmpwin);
    if (n > 0) {
        choice = selected[0].item.a_int;
        /* skip preselected entry if we have more than one item chosen */
        if (n > 1 && choice == (int) flags.discosort)
            choice = selected[1].item.a_int;
        free((genericptr_t) selected);
        flags.discosort = choice;
    }
    return n;
}

/* the '\' command - show discovered object types */
int
dodiscovered() /* free after Robert Viduya */
{
    winid tmpwin;
    char *s, *p, oclass, prev_class,
         classes[MAXOCLASSES], buf[BUFSZ],
         *sorted_lines[NUM_OBJECTS]; /* overkill */
    int i, j, sortindx, dis, ct, uniq_ct, arti_ct, sorted_ct;
    boolean alphabetized, alphabyclass, lootsort;

    if (!flags.discosort || !(p = index(disco_order_let, flags.discosort)))
        flags.discosort = 'o';

    if (iflags.menu_requested) {
        if (choose_disco_sort(1) < 0)
            return 0;
    }
    alphabyclass = (flags.discosort == 'c');
    alphabetized = (flags.discosort == 'a' || alphabyclass);
    lootsort = (flags.discosort == 's');
    sortindx = index(disco_order_let, flags.discosort) - disco_order_let;

    tmpwin = create_nhwindow(NHW_MENU);
    Sprintf(buf, "Discoveries, %s", disco_orders_descr[sortindx]);
    putstr(tmpwin, 0, buf);
    putstr(tmpwin, 0, "");

    /* gather "unique objects" into a pseudo-class; note that they'll
       also be displayed individually within their regular class */
    uniq_ct = 0;
    for (i = dis = 0; i < SIZE(uniq_objs); i++)
        if (objects[uniq_objs[i]].oc_name_known) {
            if (!dis++)
                putstr(tmpwin, iflags.menu_headings, "Unique items");
            ++uniq_ct;
            Sprintf(buf, "  %s", OBJ_NAME(objects[uniq_objs[i]]));
            putstr(tmpwin, 0, buf);
        }
    /* display any known artifacts as another pseudo-class */
    arti_ct = disp_artifact_discoveries(tmpwin);

    /* several classes are omitted from packorder; one is of interest here */
    Strcpy(classes, flags.inv_order);
    if (!index(classes, VENOM_CLASS))
        (void) strkitten(classes, VENOM_CLASS); /* append char to string */

    ct = uniq_ct + arti_ct;
    sorted_ct = 0;
    for (s = classes; *s; s++) {
        oclass = *s;
        prev_class = oclass + 1; /* forced different from oclass */
        for (i = g.bases[(int) oclass];
             i < NUM_OBJECTS && objects[i].oc_class == oclass; i++) {
            if ((dis = g.disco[i]) != 0 && interesting_to_discover(dis)) {
                ct++;
                if (oclass != prev_class) {
                    if ((alphabyclass || lootsort) && sorted_ct) {
                        /* output previous class */
                        qsort(sorted_lines, sorted_ct, sizeof (char *),
                              discovered_cmp);
                        for (j = 0; j < sorted_ct; ++j) {
                            p = sorted_lines[j];
                            if (lootsort) {
                                p[6] = p[0]; /* '*' or ' ' */
                                p += 6;
                            }
                            putstr(tmpwin, 0, p);
                            free(sorted_lines[j]), sorted_lines[j] = 0;
                        }
                        sorted_ct = 0;
                    }
                    if (!alphabetized || alphabyclass) {
                        /* header for new class */
                        putstr(tmpwin, iflags.menu_headings,
                               let_to_name(oclass, FALSE, FALSE));
                        prev_class = oclass;
                    }
                }
                Strcpy(buf,  objects[dis].oc_pre_discovered ? "* " : "  ");
                if (lootsort)
                    (void) sortloot_descr(dis, &buf[2]);
                Strcat(buf, obj_typename(dis));

                if (!alphabetized && !lootsort)
                    putstr(tmpwin, 0, buf);
                else
                    sorted_lines[sorted_ct++] = dupstr(buf);
            }
        }
    }
    if (ct == 0) {
        You("haven't discovered anything yet...");
    } else {
        if (sorted_ct) {
            /* if we're alphabetizing by class, we've already shown the
               relevant header above; if we're alphabetizing across all
               classes, we normally don't need a header; but it we showed
               any unique items or any artifacts then we do need one */
            if ((uniq_ct || arti_ct) && !alphabyclass)
                putstr(tmpwin, iflags.menu_headings, "Discovered items");
            qsort(sorted_lines, sorted_ct, sizeof (char *), discovered_cmp);
            for (j = 0; j < sorted_ct; ++j) {
                p = sorted_lines[j];
                if (lootsort) {
                    p[6] = p[0]; /* '*' or ' ' */
                    p += 6;
                }
                putstr(tmpwin, 0, p);
                free(sorted_lines[j]), sorted_lines[j] = 0;
            }
        }
        display_nhwindow(tmpwin, TRUE);
    }
    destroy_nhwindow(tmpwin);

    return 0;
}

/* lower case let_to_name() output, which differs from def_oc_syms[].name */
static char *
oclass_to_name(oclass, buf)
char oclass;
char *buf;
{
    char *s;

    Strcpy(buf, let_to_name(oclass, FALSE, FALSE));
    for (s = buf; *s; ++s)
        *s = lowc(*s);
    return buf;
}

/* the '`' command - show discovered object types for one class */
int
doclassdisco()
{
    static NEARDATA const char
        prompt[] = "View discoveries for which sort of objects?",
        havent_discovered_any[] = "haven't discovered any %s yet.",
        unique_items[] = "unique items",
        artifact_items[] = "artifacts";
    winid tmpwin = WIN_ERR;
    menu_item *pick_list = 0;
    anything any;
    char *p, *s, c, oclass, menulet, allclasses[MAXOCLASSES],
         discosyms[2 + MAXOCLASSES + 1], buf[BUFSZ],
         *sorted_lines[NUM_OBJECTS]; /* overkill */
    int i, ct, dis, xtras, sorted_ct;
    boolean traditional, alphabetized, lootsort;

    if (!flags.discosort || !(p = index(disco_order_let, flags.discosort)))
        flags.discosort = 'o';

    if (iflags.menu_requested) {
        if (choose_disco_sort(2) < 0)
            return 0;
    }
    alphabetized = (flags.discosort == 'a' || flags.discosort == 'c');
    lootsort = (flags.discosort == 's');

    discosyms[0] = '\0';
    traditional = (flags.menu_style == MENU_TRADITIONAL
                   || flags.menu_style == MENU_COMBINATION);
    if (!traditional) {
        tmpwin = create_nhwindow(NHW_MENU);
        start_menu(tmpwin, MENU_BEHAVE_STANDARD);
    }
    any = cg.zeroany;
    menulet = 'a';

    /* check whether we've discovered any unique objects */
    for (i = 0; i < SIZE(uniq_objs); i++)
        if (objects[uniq_objs[i]].oc_name_known) {
            Strcat(discosyms, "u");
            if (!traditional) {
                any.a_int = 'u';
                add_menu(tmpwin, NO_GLYPH, &any, menulet++, 0, ATR_NONE,
                         unique_items, MENU_ITEMFLAGS_NONE);
            }
            break;
        }

    /* check whether we've discovered any artifacts */
    if (disp_artifact_discoveries(WIN_ERR) > 0) {
        Strcat(discosyms, "a");
        if (!traditional) {
            any.a_int = 'a';
            add_menu(tmpwin, NO_GLYPH, &any, menulet++, 0, ATR_NONE,
                     artifact_items, MENU_ITEMFLAGS_NONE);
        }
    }

    /* collect classes with discoveries, in packorder ordering; several
       classes are omitted from packorder and one is of interest here */
    Strcpy(allclasses, flags.inv_order);
    if (!index(allclasses, VENOM_CLASS))
        (void) strkitten(allclasses, VENOM_CLASS); /* append char to string */
    /* construct discosyms[] */
    for (s = allclasses; *s; ++s) {
        oclass = *s;
        c = def_oc_syms[(int) oclass].sym;
        for (i = g.bases[(int) oclass];
             i < NUM_OBJECTS && objects[i].oc_class == oclass; ++i)
            if ((dis = g.disco[i]) != 0 && interesting_to_discover(dis)) {
                if (!index(discosyms, c)) {
                    Sprintf(eos(discosyms), "%c", c);
                    if (!traditional) {
                        any.a_int = c;
                        add_menu(tmpwin, NO_GLYPH, &any, menulet++, c,
                                 ATR_NONE, oclass_to_name(oclass, buf),
                                 MENU_ITEMFLAGS_NONE);
                    }
                }
            }
    }

    /* there might not be anything for us to do... */
    if (!discosyms[0]) {
        You(havent_discovered_any, "items");
        if (tmpwin != WIN_ERR)
            destroy_nhwindow(tmpwin);
        return 0;
    }

    /* have player choose a class */
    c = '\0'; /* class not chosen yet */
    if (traditional) {
        /* we'll prompt even if there's only one viable class; we add all
           nonviable classes as unseen acceptable choices so player can ask
           for discoveries of any class whether it has discoveries or not */
        for (s = allclasses, xtras = 0; *s; ++s) {
            c = def_oc_syms[(int) *s].sym;
            if (!index(discosyms, c)) {
                if (!xtras++)
                    (void) strkitten(discosyms, '\033');
                (void) strkitten(discosyms, c);
            }
        }
        /* get the class (via its symbol character) */
        c = yn_function(prompt, discosyms, '\0');
        savech(c);
        if (!c)
            clear_nhwindow(WIN_MESSAGE);
    } else {
        /* menustyle:full or menustyle:partial */
        if (!discosyms[1] && flags.menu_style == MENU_PARTIAL) {
            /* only one class; menustyle:partial normally jumps past class
               filtering straight to final menu so skip class filter here */
            c = discosyms[0];
        } else {
            /* more than one choice, or menustyle:full which normally has
               an intermediate class selection menu before the final menu */
            end_menu(tmpwin, prompt);
            i = select_menu(tmpwin, PICK_ONE, &pick_list);
            if (i > 0) {
                c = pick_list[0].item.a_int;
                free((genericptr_t) pick_list);
            } /* else c stays 0 */
        }
        destroy_nhwindow(tmpwin);
    }
    if (!c)
        return 0; /* player declined to make a selection */

    /*
     * show discoveries for object class c
     */
    tmpwin = create_nhwindow(NHW_MENU);
    ct = 0;
    switch (c) {
    case 'u':
        putstr(tmpwin, iflags.menu_headings,
               upstart(strcpy(buf, unique_items)));
        for (i = 0; i < SIZE(uniq_objs); i++)
            if (objects[uniq_objs[i]].oc_name_known) {
                ++ct;
                Sprintf(buf, "  %s", OBJ_NAME(objects[uniq_objs[i]]));
                putstr(tmpwin, 0, buf);
            }
        if (!ct)
            You(havent_discovered_any, unique_items);
        break;
    case 'a':
        /* disp_artifact_discoveries() includes a header */
        ct = disp_artifact_discoveries(tmpwin);
        if (!ct)
            You(havent_discovered_any, artifact_items);
        break;
    default:
        oclass = def_char_to_objclass(c);
        Sprintf(buf, "Discovered %s in %s", let_to_name(oclass, FALSE, FALSE),
                (flags.discosort == 'o') ? "order of discovery"
                : (flags.discosort == 's') ? "'sortloot' order"
                  : "alphabetical order");
        putstr(tmpwin, 0, buf); /* skip iflags.menu_headings */
        sorted_ct = 0;
        for (i = g.bases[(int) oclass]; i < g.bases[oclass + 1] - 1; ++i) {
            if ((dis = g.disco[i]) != 0 && interesting_to_discover(dis)) {
                ++ct;
                Strcpy(buf,  objects[dis].oc_pre_discovered ? "* " : "  ");
                if (lootsort)
                    (void) sortloot_descr(dis, &buf[2]);
                Strcat(buf, obj_typename(dis));

                if (!alphabetized && !lootsort)
                    putstr(tmpwin, 0, buf);
                else
                    sorted_lines[sorted_ct++] = dupstr(buf);
            }
        }
        if (!ct) {
            You(havent_discovered_any, oclass_to_name(oclass, buf));
        } else if (sorted_ct) {
            qsort(sorted_lines, sorted_ct, sizeof (char *), discovered_cmp);
            for (i = 0; i < sorted_ct; ++i) {
                p = sorted_lines[i];
                if (lootsort) {
                    p[6] = p[0]; /* '*' or ' ' */
                    p += 6;
                }
                putstr(tmpwin, 0, p);
                free(sorted_lines[i]), sorted_lines[i] = 0;
            }
        }
        break;
    }
    if (ct)
        display_nhwindow(tmpwin, TRUE);
    destroy_nhwindow(tmpwin);
    return 0;
}

/* put up nameable subset of discoveries list as a menu */
void
rename_disco()
{
    register int i, dis;
    int ct = 0, mn = 0, sl;
    char *s, oclass, prev_class;
    winid tmpwin;
    anything any;
    menu_item *selected = 0;

    any = cg.zeroany;
    tmpwin = create_nhwindow(NHW_MENU);
    start_menu(tmpwin, MENU_BEHAVE_STANDARD);

    /*
     * Skip the "unique objects" section (each will appear within its
     * regular class if it is nameable) and the artifacts section.
     * We assume that classes omitted from packorder aren't nameable
     * so we skip venom too.
     */

    /* for each class, show discoveries in that class */
    for (s = flags.inv_order; *s; s++) {
        oclass = *s;
        prev_class = oclass + 1; /* forced different from oclass */
        for (i = g.bases[(int) oclass];
             i < NUM_OBJECTS && objects[i].oc_class == oclass; i++) {
            dis = g.disco[i];
            if (!dis || !interesting_to_discover(dis))
                continue;
            ct++;
            if (!objtyp_is_callable(dis))
                continue;
            mn++;

            if (oclass != prev_class) {
                any.a_int = 0;
                add_menu(tmpwin, NO_GLYPH, &any, 0, 0, iflags.menu_headings,
                         let_to_name(oclass, FALSE, FALSE),
                         MENU_ITEMFLAGS_NONE);
                prev_class = oclass;
            }
            any.a_int = dis;
            add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE,
                     obj_typename(dis), MENU_ITEMFLAGS_NONE);
        }
    }
    if (ct == 0) {
        You("haven't discovered anything yet...");
    } else if (mn == 0) {
        pline("None of your discoveries can be assigned names...");
    } else {
        end_menu(tmpwin, "Pick an object type to name");
        dis = STRANGE_OBJECT;
        sl = select_menu(tmpwin, PICK_ONE, &selected);
        if (sl > 0) {
            dis = selected[0].item.a_int;
            free((genericptr_t) selected);
        }
        if (dis != STRANGE_OBJECT) {
            struct obj odummy;

            odummy = cg.zeroobj;
            odummy.otyp = dis;
            odummy.oclass = objects[dis].oc_class;
            odummy.quan = 1L;
            odummy.known = !objects[dis].oc_uses_known;
            odummy.dknown = 1;
            docall(&odummy);
        }
    }
    destroy_nhwindow(tmpwin);
    return;
}

/*o_init.c*/

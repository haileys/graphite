/*  GRAPHITE2 LICENSING

    Copyright 2010, SIL International
    All rights reserved.

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should also have received a copy of the GNU Lesser General Public
    License along with this library in the file named "LICENSE".
    If not, write to the Free Software Foundation, Inc., 59 Temple Place, 
    Suite 330, Boston, MA 02111-1307, USA or visit their web page on the 
    internet at http://www.fsf.org/licenses/lgpl.html.
*/
#include "Segment.h"
#include "Slot.h"
#include "CharInfo.h"
#include "Rule.h"


using namespace graphite2;

Slot::Slot() :
    m_next(NULL), m_prev(NULL),
    m_glyphid(0), m_realglyphid(0), m_original(0), m_before(0), m_after(0),
    m_parent(NULL), m_child(NULL), m_sibling(NULL),
    m_position(0, 0), m_shift(0, 0), m_advance(-1, -1),
    m_attach(0, 0), m_with(0, 0),
    m_flags(0)
    // Do not set m_userAttr since it is set *before* new is called since this
    // is used as a positional new to reset the GrSlot
{
}

// take care, this does not copy any of the GrSlot pointer fields
void Slot::set(const Slot & orig, int charOffset, uint8 numUserAttr)
{
    // leave m_next and m_prev unchanged
    m_glyphid = orig.m_glyphid;
    m_realglyphid = orig.m_realglyphid;
    m_original = orig.m_original + charOffset;
    m_before = orig.m_before + charOffset;
    m_after = orig.m_after + charOffset;
    m_parent = NULL;
    m_child = NULL;
    m_sibling = NULL;
    m_position = orig.m_position;
    m_shift = orig.m_shift;
    m_advance = orig.m_advance;
    m_attach = orig.m_attach;
    m_with = orig.m_with;
    m_flags = orig.m_flags;
    m_attLevel = orig.m_attLevel;
    assert(!orig.m_userAttr || m_userAttr);
    if (m_userAttr && orig.m_userAttr)
    {
        memcpy(m_userAttr, orig.m_userAttr, numUserAttr * sizeof(*m_userAttr));
    }
}

void Slot::update(int /*numGrSlots*/, int numCharInfo, Position &relpos)
{
    m_before += numCharInfo;
    m_after += numCharInfo;
    m_position = m_position + relpos;
};

Position Slot::finalise(const Segment *seg, const Font *font, Position *base, Rect *bbox, float *cMin, uint8 attrLevel, float * clusterMin)
{
    if (attrLevel && m_attLevel > attrLevel) return Position(0, 0);
    float scale = 1.0;
    Position shift = m_shift;
    float tAdvance = m_advance.x;
    const GlyphFace * glyphFace = seg->getFace()->getGlyphFaceCache()->glyphSafe(glyph());
    if (font)
    {
        scale = font->scale();
        shift *= scale;
        if (font->isHinted())
        {
            if (glyphFace)
                tAdvance = (m_advance.x - glyphFace->theAdvance().x) * scale + font->advance(m_glyphid);
            else
                tAdvance = (m_advance.x - seg->glyphAdvance(glyph())) * scale + font->advance(m_glyphid);
        }
        else
            tAdvance *= scale;
    }    
    Position res;

    m_position = *base + shift;
    if (!m_parent)
    {
        res = *base + Position(tAdvance, m_advance.y * scale);
        *cMin = 0.;
        *clusterMin = base->x;
    }
    else
    {
        float tAdv;
        m_position += (m_attach - m_with) * scale;
        tAdv = tAdvance > 0.f ? m_position.x + tAdvance - shift.x : 0.f;
        res = Position(tAdv, 0);
        if (m_position.x < *clusterMin) *clusterMin = m_position.x;
    }

    if (glyphFace)
    {
        Rect ourBbox = glyphFace->theBBox() * scale + m_position;
        *bbox = bbox->widen(ourBbox);
    }
    //Rect ourBbox = seg->theGlyphBBoxTemporary(glyph()) * scale + m_position;
    //bbox->widen(ourBbox);

    if (m_parent && m_position.x < *cMin) *cMin = m_position.x;

    if (m_child)
    {
        Position tRes = m_child->finalise(seg, font, &m_position, bbox, cMin, attrLevel, clusterMin);
        if (tRes.x > res.x) res = tRes;
    }

    if (m_parent && m_sibling)
    {
        Position tRes = m_sibling->finalise(seg, font, base, bbox, cMin, attrLevel, clusterMin);
        if (tRes.x > res.x) res = tRes;
    }
    
    if (!m_parent)
    {
        if (*cMin < 0)
        {
            Position adj = Position(-*cMin, 0.);
            res += adj;
            m_position += adj;
            if (m_child) m_child->floodShift(adj);
        }
        else if ((seg->dir() & 1) && (*clusterMin < base->x))
        {
            Position adj = Position(base->x - *clusterMin, 0.);
            res += adj;
            m_position += adj;
            if (m_child) m_child->floodShift(adj);
        }
    }
    return res;
}

uint32 Slot::clusterMetric(const Segment *seg, uint8 metric, uint8 attrLevel)
{
    Position base;
    Rect bbox = seg->theGlyphBBoxTemporary(gid());
    float cMin = 0.;
    float clusterMin = 0.;
    Position res = finalise(seg, NULL, &base, &bbox, &cMin, attrLevel, &clusterMin);

    switch ((enum metrics)metric)
    {
    case kgmetLsb :
        return static_cast<uint32>(bbox.bl.x);
    case kgmetRsb :
        return static_cast<uint32>(res.x - bbox.tr.x);
    case kgmetBbTop :
        return static_cast<uint32>(bbox.tr.y);
    case kgmetBbBottom :
        return static_cast<uint32>(bbox.bl.y);
    case kgmetBbLeft :
        return static_cast<uint32>(bbox.bl.x);
    case kgmetBbRight :
        return static_cast<uint32>(bbox.tr.x);
    case kgmetBbWidth :
        return static_cast<uint32>(bbox.tr.x - bbox.bl.x);
    case kgmetBbHeight :
        return static_cast<uint32>(bbox.tr.y - bbox.bl.y);
    case kgmetAdvWidth :
        return static_cast<uint32>(res.x);
    case kgmetAdvHeight :
        return static_cast<uint32>(res.y);
    default :
        return 0;
    }
}

int Slot::getAttr(const Segment *seg, attrCode index, uint8 subindex) const
{
    if (!this) return 0;
    if (index == gr_slatUserDefnV1)
    {
        index = gr_slatUserDefn;
        subindex = 0;
    }
    switch (index)
    {
    case gr_slatAdvX :
        return static_cast<int>(m_advance.x);
    case gr_slatAdvY :
        return static_cast<int>(m_advance.y);
    case gr_slatAttTo :
        return 0;
    case gr_slatAttX :
        return static_cast<int>(m_attach.x);
    case gr_slatAttY :
        return static_cast<int>(m_attach.y);
    case gr_slatAttXOff :
        return 0;
    case gr_slatAttYOff :
        return 0;
    case gr_slatAttWithX :
        return static_cast<int>(m_with.x);
    case gr_slatAttWithY :
        return static_cast<int>(m_with.y);
    case gr_slatAttWithXOff :
        return 0;
    case gr_slatAttWithYOff :
        return 0;
    case gr_slatAttLevel :
        return m_attLevel;
    case gr_slatBreak :
        return seg->charinfo(m_original)->breakWeight();
    case gr_slatCompRef :
        return 0;
    case gr_slatDir :
        return seg->dir();
    case gr_slatInsert :
        return isInsertBefore();
    case gr_slatPosX :
        return static_cast<int>(m_position.x); // but need to calculate it
    case gr_slatPosY :
        return static_cast<int>(m_position.y);
    case gr_slatShiftX :
        return static_cast<int>(m_shift.x);
    case gr_slatShiftY :
        return static_cast<int>(m_shift.y);
    case gr_slatMeasureSol :
        return -1; // err what's this?
    case gr_slatMeasureEol :
        return -1;
    case gr_slatJStretch :
        return 0;
    case gr_slatJShrink :
        return 0;
    case gr_slatJStep :
        return 0;
    case gr_slatJWeight :
        return 0;
    case gr_slatJWidth :
        return 0;
    case gr_slatUserDefn :
        return m_userAttr[subindex];
    default :
        return 0;
    }
}

void Slot::setAttr(Segment *seg, attrCode index, uint8 subindex, int16 value, const SlotMap & map)
{
    if (!this) return;
    if (index == gr_slatUserDefnV1)
    {
        index = gr_slatUserDefn;
        subindex = 0;
    }
    switch (index)
    {
    case gr_slatAdvX :
        m_advance = Position(value, m_advance.y);
        break;
    case gr_slatAdvY :
        m_advance = Position(m_advance.x, value);
        break;
    case gr_slatAttTo :
    {
        const uint16 idx = uint16(value);
        if (idx < map.size())
        {
            Slot *other = map[idx];
            attachTo(other);
            other->child(this);
            m_attach = Position(seg->glyphAdvance(other->gid()), 0);
        }
        else
        {
#ifndef DISABLE_TRACING
            XmlTraceLog::get().warning("invalid slatAttTo %d", value);
#endif
        }
        break;
    }
    case gr_slatAttX :
        m_attach = Position(value, m_attach.y);
        break;
    case gr_slatAttY :
        m_attach = Position(m_attach.x, value);
        break;
    case gr_slatAttXOff :
        break;
    case gr_slatAttYOff :
        break;
    case gr_slatAttWithX :
        m_with = Position(value, m_with.y);
        break;
    case gr_slatAttWithY :
        m_with = Position(m_with.x, value);
        break;
    case gr_slatAttWithXOff :
        break;
    case gr_slatAttWithYOff :
        break;
    case gr_slatAttLevel :
        assert(value < 128);
        assert(value > -128);
        m_attLevel = static_cast<byte>(value);
        break;
    case gr_slatBreak :
        seg->charinfo(m_original)->breakWeight(value);
        break;
    case gr_slatCompRef :
        break;      // not sure what to do here
    case gr_slatDir :
        break;  // read only
    case gr_slatInsert :
        markInsertBefore(value? true : false);
        break;
    case gr_slatPosX :
        break; // can't set these here
    case gr_slatPosY :
        break;
    case gr_slatShiftX :
        m_shift = Position(value, m_shift.y);
        break;
    case gr_slatShiftY :
        m_shift = Position(m_shift.x, value);
        break;
    case gr_slatMeasureSol :
        break;
    case gr_slatMeasureEol :
        break;
    case gr_slatJStretch :
        break;  // handle these later
    case gr_slatJShrink :
        break;
    case gr_slatJStep :
        break;
    case gr_slatJWeight :
        break;
    case gr_slatJWidth :
        break;
    case gr_slatUserDefn :
        m_userAttr[subindex] = value;
        break;
    default :
        break;
    }
}

void Slot::child(Slot *ap)
{
    if (ap == m_child) {}
    else if (!m_child)
        m_child = ap;
    else
        m_child->sibling(ap);
}

void Slot::sibling(Slot *ap)
{
    if (ap == m_sibling) {}
    else if (!m_sibling)
        m_sibling = ap;
    else
        m_sibling->sibling(ap);
}

void Slot::setGlyph(Segment *seg, uint16 glyphid, const GlyphFace * theGlyph)
{
    m_glyphid = glyphid;
    if (!theGlyph)
    {
        theGlyph = seg->getFace()->getGlyphFaceCache()->glyphSafe(glyphid);
        if (!theGlyph)
        {
            m_realglyphid = 0;
            m_advance = Position(0.,0.);
            return;
        }
    }
    m_realglyphid = theGlyph->getAttr(seg->silf()->aPseudo());
    if (m_realglyphid)
        theGlyph = seg->getFace()->getGlyphFaceCache()->glyphSafe(m_realglyphid);
    m_advance = Position(theGlyph->theAdvance().x, 0.);
}

void Slot::floodShift(Position adj)
{
    m_position += adj;
    if (m_child) m_child->floodShift(adj);
    if (m_sibling) m_sibling->floodShift(adj);
}

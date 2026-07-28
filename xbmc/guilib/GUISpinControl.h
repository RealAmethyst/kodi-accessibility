/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

/*!
\file GUISpinControl.h
\brief
*/

#include "GUIControl.h"
#include "GUILabel.h"
#include "GUITexture.h"

#include <limits>
#include <vector>

#define SPIN_CONTROL_TYPE_INT    1
#define SPIN_CONTROL_TYPE_FLOAT  2
#define SPIN_CONTROL_TYPE_TEXT   3
#define SPIN_CONTROL_TYPE_PAGE   4

/*!
 \ingroup controls
 \brief
 */
class CGUISpinControl : public CGUIControl
{
public:
  CGUISpinControl(int parentID, int controlID, float posX, float posY, float width, float height, const CTextureInfo& textureUp, const CTextureInfo& textureDown, const CTextureInfo& textureUpFocus, const CTextureInfo& textureDownFocus, const CTextureInfo& textureUpDisabled, const CTextureInfo& textureDownDisabled, const CLabelInfo& labelInfo, int iType);
  ~CGUISpinControl() override = default;
  CGUISpinControl* Clone() const override { return new CGUISpinControl(*this); }

  void Process(unsigned int currentTime, CDirtyRegionList &dirtyregions) override;

  /*!
   * \brief Notices the value changing, for accessibility.
   *
   * Turning a spinner moves no focus and changes no list selection, so nothing else in the
   * patch emits anything for it -- a screen reader could only find out by polling, which is
   * what this replaces.
   *
   * In DoProcess() rather than Process() on purpose. CGUISpinControlEx does call the base
   * Process(), but CGUIPanelContainer does not, and that cost a silent failure across every
   * poster view once already; nothing overrides DoProcess(), so hooking here cannot be
   * broken by a subclass on a future rebase. By this point CGUIControl::DoProcess has
   * already run Process(), so the value is settled.
   */
  void DoProcess(unsigned int currentTime, CDirtyRegionList& dirtyregions) override;

  void Render() override;
  bool OnAction(const CAction &action) override;
  void OnLeft() override;
  void OnRight() override;
  bool HitTest(const CPoint &point) const override;
  bool OnMouseOver(const CPoint &point) override;
  bool OnMessage(CGUIMessage& message) override;
  void AllocResources() override;
  void FreeResources(bool immediately = false) override;
  void DynamicResourceAlloc(bool bOnOff) override;
  void SetInvalid() override;
  void SetPosition(float posX, float posY) override;
  float GetWidth() const override;
  void SetRange(int iStart, int iEnd);
  void SetFloatRange(float fStart, float fEnd);
  void SetValue(int iValue);
  void SetValueFromLabel(const std::string &label);
  void SetFloatValue(float fValue);
  void SetStringValue(const std::string& strValue);
  int GetValue() const;
  float GetFloatValue() const;
  std::string GetStringValue() const;
  void AddLabel(const std::string& strLabel, int iValue);
  void AddLabel(const std::string& strLabel, const std::string& strValue);
  const std::string GetLabel() const;
  void SetReverse(bool bOnOff);
  int GetMaximum() const;
  int GetMinimum() const;
  void SetSpinAlign(uint32_t align, float offsetX)
  {
    m_label.GetLabelInfo().align = align;
    m_label.GetLabelInfo().offsetX = offsetX;
  }
  void SetType(int iType) { m_iType = iType; }
  float GetSpinWidth() const { return m_imgspinUp->GetWidth(); }
  float GetSpinHeight() const { return m_imgspinUp->GetHeight(); }
  void SetFloatInterval(float fInterval);
  void SetShowRange(bool bOnoff) ;
  void SetShowOnePage(bool showOnePage) { m_showOnePage = showOnePage; }
  void Clear();
  std::string GetDescription() const override;
  bool IsFocusedOnUp() const;

  bool IsVisible() const override;

protected:
  CGUISpinControl(const CGUISpinControl& control);

  EVENT_RESULT OnMouseEvent(const CPoint& point, const KODI::MOUSE::CMouseEvent& event) override;
  bool UpdateColors(const CGUIListItem* item) override;
  /*! \brief Render the spinner text
   \param posX position of the left edge of the text
   \param posY positing of the top edge of the text
   \param width width of the text
   \param height height of the text
   */
  virtual void RenderText(float posX, float posY, float width, float height);
  CGUILabel::COLOR GetTextColor() const;
  void PageUp();
  void PageDown();
  bool CanMoveDown(bool bTestReverse = true);
  bool CanMoveUp(bool bTestReverse = true);
  void MoveUp(bool bTestReverse = true);
  void MoveDown(bool bTestReverse = true);
  void ChangePage(int amount);
  int m_iStart;
  int m_iEnd;
  float m_fStart;
  float m_fEnd;
  int m_iValue;
  float m_fValue;
  int m_iType;
  int m_iSelect;

  /*!
   * \brief What the accessibility layer last reported, so only real changes are announced.
   *
   * Both are compared, because which one carries the value depends on m_iType: a text
   * spinner steps m_iValue as an index into its labels, an integer one uses it as the value
   * itself, and a float one uses m_fValue. Deliberately the raw fields rather than
   * GetValue(), which maps a text spinner's index through m_vecValues and so can report the
   * same number for two different labels.
   *
   * Initialised to something no real value can be, so the first announcement after gaining
   * focus is never suppressed.
   */
  int m_lastAnnouncedValue{std::numeric_limits<int>::min()};
  float m_lastAnnouncedFloat{std::numeric_limits<float>::lowest()};
  bool m_bReverse;
  float m_fInterval;
  std::vector<std::string> m_vecLabels;
  std::vector<int> m_vecValues;
  std::vector<std::string> m_vecStrValues;
  std::unique_ptr<CGUITexture> m_imgspinUp;
  std::unique_ptr<CGUITexture> m_imgspinDown;
  std::unique_ptr<CGUITexture> m_imgspinUpFocus;
  std::unique_ptr<CGUITexture> m_imgspinDownFocus;
  std::unique_ptr<CGUITexture> m_imgspinUpDisabled;
  std::unique_ptr<CGUITexture> m_imgspinDownDisabled;
  CGUILabel   m_label;
  bool m_bShowRange;
  char m_szTyped[10];
  int m_iTypedPos;

  int m_currentItem;
  int m_itemsPerPage;
  int m_numItems;
  bool m_showOnePage;
};


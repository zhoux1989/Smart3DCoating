//===================================================================================================
// Summary:
//		操作结果确认对话框
// Date:
//		2017-04-11
// Author:
//		
//===================================================================================================

#include "stdafx.h"
#include "DlgSetThick.h"
#include "PreGlobal.h"
#include "DlgMergeDir.h"

//===================================================================================================

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//===================================================================================================

CDlgSetThick::CDlgSetThick(CWnd* pParent /*=NULL*/) : CDialog(CDlgSetThick::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDlgSetThick)
	m_dMaxThick = 0.0;
	m_pAsm = NULL;
	m_nOffsetOrient = 1;
	m_bNoChange = TRUE;
	//}}AFX_DATA_INIT
}

void CDlgSetThick::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDlgSetThick)
	DDX_Control(pDX, IDC_LIST_SURF, m_listSurf);
	//}}AFX_DATA_MAP	
}

BEGIN_MESSAGE_MAP(CDlgSetThick, CDialog)
	//{{AFX_MSG_MAP(CDlgSetThick)
	ON_WM_MOVE()
	ON_BN_CLICKED(IDC_BTN_SEL, OnBnClickedSel)
	ON_LBN_SELCHANGE(IDC_LIST_SURF, OnLbnSelchangeListSurf)
	ON_LBN_DBLCLK(IDC_LIST_SURF, OnLbnDblclkListSurf)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CDlgSetThick::OnMove(int x, int y) 
{
	CDialog::OnMove(x, y);
}

BOOL CDlgSetThick::OnInitDialog() 
{
	CDialog::OnInitDialog();

	CString str;
	for (int i=0; i<m_arrQltfaceData.size(); i++)
	{
		str.Format(L"曲面:%d (thick=%.2f)", m_arrQltfaceData[i].nSurfID, m_arrQltfaceData[i].dOffset);
		m_listSurf.AddString(str);
	}

	return TRUE;
}

// 第二步：两两相交
// 第三步：创建相交线
// 第四步：裁剪
// 第五步：合并
// 第六步：实体化

void CDlgSetThick::OnOK()
{
	ShowWindow(SW_HIDE);

	ProMdl pMdlPart = m_itemQuiltBase.owner;
	if (pMdlPart != NULL)
	{
		int nQltfaceCount = (int)m_arrQltfaceData.size();
		ProSelection selQuiltBase;
		ProSelectionAlloc(NULL, &m_itemQuiltBase, &selQuiltBase);
		// 检查是否相等
		for (int i=0; i<nQltfaceCount; i++)
		{
			if (!DEQUAL(m_arrQltfaceData[i].dOffset, m_dMaxThick))
			{
				m_bNoChange = FALSE;
				break;
			}
		}
		if (!m_bNoChange)
		{
			vector<int> arrFeatID;				// 记录过程中产生的全部特征ID
			int* arrHideFeatID = NULL;
			ProArrayAlloc(0, sizeof(int), 1, (ProArray*)&arrHideFeatID);
			
			// 第一步：为面组中的所有子面创建偏离面，并与偏离前的面建立关联
			ProQuilt quiltBase;				// 基础包覆面
			ProGeomitemToQuilt(&m_itemQuiltBase, &quiltBase);
			for (int i=0; i<nQltfaceCount; i++)
			{
				// 获取面组中每个子面的邻面信息
				GetNeighborSurfInQuilt(pMdlPart, m_arrQltfaceData[i].qltface, quiltBase, m_arrQltfaceData[i].arrNeighborface);

				// 对每个子面进行偏离
				ProModelitem itemQltface;
				ProSurfaceToGeomitem(ProMdlToSolid(pMdlPart), m_arrQltfaceData[i].qltface, &itemQltface);
				ProSelection selQltface = NULL;
				ProSelectionAlloc(NULL, &itemQltface, &selQltface);
				int nFeatID = OffsetSurface(pMdlPart, selQltface, m_arrQltfaceData[i].dOffset*m_nOffsetOrient, m_arrQltfaceData[i].itemOffsetQlt);
				if (nFeatID > 0)
				{
					arrFeatID.push_back(nFeatID);
					ProArrayObjectAdd((ProArray*)&arrHideFeatID, 0, 1, &nFeatID);

					// 获取偏离后的面上的一个点
					/*ProModelitem itemOffsetSurf;
					QuiltToSurf(m_arrQltfaceData[i].itemOffsetQlt, itemOffsetSurf);
					ProSurface surfOffset;
					ProGeomitemToSurface(&itemOffsetSurf, &surfOffset);
					GetPointOnSurface(pMdlPart, surfOffset, m_arrQltfaceData[i].pntOnOffset);*/
				}
				
				/*mapQuiltSub.insert(make_pair(m_arrQltfaceData[i].nSurfID, subData));
				mapExtendQuiltID.insert(make_pair(subData.itemExtendQuilt.id, m_arrQltfaceData[i].nSurfID));*/
			}

			// 第二步：对偏离面进行延展，两两相交
			for (int i=0; i<nQltfaceCount; i++)
			{
				// 偏离后的子面
				for (int j=0, nNeighborIndex = -1; j<m_arrQltfaceData[i].arrNeighborface.size(); j++)
				{
					// 偏离后的子面的邻面
					nNeighborIndex = m_mapQltfaceIndex[m_arrQltfaceData[i].arrNeighborface[j]];

					int nFeatID1, nFeatID2;
					if (ExtendToInterSect(pMdlPart, m_arrQltfaceData[i].itemOffsetQlt, 
						m_arrQltfaceData[nNeighborIndex].itemOffsetQlt, nFeatID1, nFeatID2))
					{
						arrFeatID.push_back(nFeatID1);
						arrFeatID.push_back(nFeatID2);
					}
				}
			}

			// 第三步：每两个相交面之间创建相交线，分别在相交线的位置裁剪
			vector<vector<int>> arrIntersectFlag(nQltfaceCount, vector<int>(nQltfaceCount));
			ProFeature featTrim;
			featTrim.type = PRO_FEATURE;
			featTrim.owner = pMdlPart;
			for (int i=0; i<nQltfaceCount; i++)
			{
				// 偏离后的子面
				ProSelection selOffsetQlt;
				ProSelectionAlloc(NULL, &m_arrQltfaceData[i].itemOffsetQlt, &selOffsetQlt);
				ProModelitem itemOffsetSurf;
				QuiltToSurf(m_arrQltfaceData[i].itemOffsetQlt, itemOffsetSurf);
				ProSurface surfOffset;
				ProGeomitemToSurface(&itemOffsetSurf, &surfOffset);

				for (int j=0, nNeighborIndex = -1; j<m_arrQltfaceData[i].arrNeighborface.size(); j++)
				{
					nNeighborIndex = m_mapQltfaceIndex[m_arrQltfaceData[i].arrNeighborface[j]];
					if (arrIntersectFlag[i][nNeighborIndex] != 1)
					{
						// 偏离后的子面的邻面
						ProSelection selNeighborQlt;
						ProSelectionAlloc(NULL, &m_arrQltfaceData[nNeighborIndex].itemOffsetQlt, &selNeighborQlt);
						ProModelitem itemCurve;
						int nFeatIntersect = CreateCurveByIntersect(pMdlPart, selOffsetQlt, selNeighborQlt, itemCurve);
						if (nFeatIntersect > 0)
						{
							ProArrayObjectAdd((ProArray*)&arrHideFeatID, 0, 1, &nFeatIntersect);

							arrIntersectFlag[i][nNeighborIndex] = arrIntersectFlag[nNeighborIndex][i] = 1;

							// 以相交线进行裁剪，裁剪时判断基准点是否在裁剪后的面上，如果不在，反向
							ProSelection selCurve;
							ProSelectionAlloc(NULL, &itemCurve, &selCurve);

							ProModelitem itemCopyContour1;
							int nFeatCopyContour1 = CreateCurveByCopyContour(pMdlPart, selOffsetQlt, itemCopyContour1);
							ProArrayObjectAdd((ProArray*)&arrHideFeatID, 0, 1, &nFeatCopyContour1);

							// 计算裁剪前的面积
							double dAreaBefor, dAreaAfter;
							ProSurfaceAreaEval(surfOffset, &dAreaBefor);
							
							BOOL bOK = TRUE;
							int nfeatArray[1];
							ProFeatureDeleteOptions opt[] = {PRO_FEAT_DELETE_NO_OPTS};
							ProSelection selCopyContour1;
							ProSelectionAlloc(NULL, &itemCopyContour1, &selCopyContour1);
							if (!TrimQuiltByExtendCurveToContour(pMdlPart, selOffsetQlt, selCurve, selCopyContour1, 0, featTrim))
							{
								if (featTrim.id > 0)
								{
									nfeatArray[0] = featTrim.id;
									ProFeatureDelete(ProMdlToSolid(pMdlPart), nfeatArray, 1, opt, 1);
								}

								if (!TrimQuiltByExtendCurve(pMdlPart, selOffsetQlt, selCurve, selCopyContour1, 0, featTrim))
								{
									if (featTrim.id > 0)
									{
										if (!ReverseTrimDirection(featTrim))
										{
											bOK = FALSE;
										}
									}
								}
							}
							if (bOK)
							{					
								if (!CheckIntersectAfterTrim(m_arrQltfaceData[i]))
								{
									ReverseTrimDirection(featTrim);
								}
								else
								{
									// 计算裁剪前的面积
									ProSurfaceAreaEval(surfOffset, &dAreaAfter);
									if (dAreaAfter/dAreaBefor < 0.2)
										ReverseTrimDirection(featTrim);
								}

								ProArrayObjectAdd((ProArray*)&arrHideFeatID, 0, 1, &featTrim.id);

								/*if (!IsPntInsideSurf(pMdlPart, surfOffset, m_arrQltfaceData[i].pntOnOffset))
								{
									ReverseTrimDirection(featTrim);
								}*/
							}

							// 计算裁剪前的面积
							ProModelitem itemSurfNeighbor;
							QuiltToSurf(m_arrQltfaceData[nNeighborIndex].itemOffsetQlt, itemSurfNeighbor);
							ProSurface surfNeighbor;
							ProGeomitemToSurface(&itemSurfNeighbor, &surfNeighbor);
							ProSurfaceAreaEval(surfNeighbor, &dAreaBefor);

							ProModelitem itemCopyContour2;
							int nFeatCopyContour2 = CreateCurveByCopyContour(pMdlPart, selNeighborQlt, itemCopyContour2);
							ProArrayObjectAdd((ProArray*)&arrHideFeatID, 0, 1, &nFeatCopyContour2);

							ProSelection selCopyContour2;
							ProSelectionAlloc(NULL, &itemCopyContour2, &selCopyContour2);
							bOK = TRUE;
							if (!TrimQuiltByExtendCurveToContour(pMdlPart, selNeighborQlt, selCurve, selCopyContour2, 0, featTrim))
							{
								if (featTrim.id > 0)
								{
									nfeatArray[0] = featTrim.id;
									ProFeatureDelete(ProMdlToSolid(pMdlPart), nfeatArray, 1, opt, 1);
								}

								if (!TrimQuiltByExtendCurve(pMdlPart, selNeighborQlt, selCurve, selCopyContour2, 0, featTrim))
								{
									if (featTrim.id > 0)
									{
										if (!ReverseTrimDirection(featTrim))
										{
											bOK = FALSE;
										}
									}
								}
							}
							if (bOK)
							{
								// 如果裁剪后与其他相邻偏离面不相交，则反向
								if (!CheckIntersectAfterTrim(m_arrQltfaceData[nNeighborIndex]))
									ReverseTrimDirection(featTrim);
								else
								{
									// 计算裁剪前的面积
									ProSurfaceAreaEval(surfNeighbor, &dAreaAfter);
									if (dAreaAfter/dAreaBefor < 0.2)
										ReverseTrimDirection(featTrim);
								}
								/*if (!IsPntInsideSurf(pMdlPart, surfNeighbor, m_arrQltfaceData[nNeighborIndex].pntOnOffset))
								{
									ReverseTrimDirection(featTrim);
								}*/
								ProArrayObjectAdd((ProArray*)&arrHideFeatID, 0, 1, &featTrim.id);
							}

							/*ProModelitemHide(&itemCopyContour1);
							ProModelitemHide(&itemCopyContour2);*/
						}
					}
				}
			}

			// 第四步：将裁剪后的面进行合并（按相邻关系进行排序）
			vector<int> arrAddFlag(nQltfaceCount);
			arrAddFlag[0] = 1;
			vector<int> arrMergeIndex;
			arrMergeIndex.push_back(0);
			for (int i=0; i<arrMergeIndex.size(); i++)
			{
				for (int j=0, nNeighborIndex=-1; j<m_arrQltfaceData[arrMergeIndex[i]].arrNeighborface.size(); j++)
				{
					nNeighborIndex = m_mapQltfaceIndex[m_arrQltfaceData[arrMergeIndex[i]].arrNeighborface[j]];
					if (arrAddFlag[nNeighborIndex] != 1)
					{
						arrMergeIndex.push_back(nNeighborIndex);
						arrAddFlag[nNeighborIndex] = 1;
					}
				}
			}
			vector<ProSelection> arrSelMergeQuilt;
			for (int i=0; i<(int)arrMergeIndex.size(); i++)
			{
				ProSelection selOffsetQlt = NULL;
				ProSelectionAlloc(NULL, &m_arrQltfaceData[arrMergeIndex[i]].itemOffsetQlt, &selOffsetQlt);
				arrSelMergeQuilt.push_back(selOffsetQlt);
			}

			ProFeature featMerge;
			int nMergeFeatID = MergeQuilts(pMdlPart, arrSelMergeQuilt, PRO_SRF_MRG_INTSCT, featMerge);
			if (nMergeFeatID > 0)
				ProArrayObjectAdd((ProArray*)&arrHideFeatID, 0, 1, &nMergeFeatID);

			// 第五步：整体按最大偏离值进行加厚
			
			ThickQuilt(pMdlPart, selQuiltBase, m_dMaxThick*m_nOffsetOrient);

			// 第六步：实体化（裁剪实体）
			int nFeatID = CreateSolidify(pMdlPart, arrSelMergeQuilt[0], m_nOffsetOrient*(-1));
			if (nFeatID > 0)
			{
				arrFeatID.push_back(nFeatID);
			}

			// 隐藏特征
			int nFeatCount = 0;
			ProArraySizeGet(arrHideFeatID, &nFeatCount);
			ProFeature featHide;
			featHide.type = PRO_FEATURE;
			featHide.owner = pMdlPart;
			for (int i=0; i<nFeatCount; i++)
			{
				featHide.id = arrHideFeatID[i];
				ProModelitemHide(&featHide);
			}
			ProArrayFree((ProArray*)&arrHideFeatID);
			CreateFeatGroup(pMdlPart, arrFeatID, L"包覆体");
		}
		else
		{
			// 直接创建等厚特征
			ThickQuilt(pMdlPart, selQuiltBase, m_dMaxThick*m_nOffsetOrient);
		}
	}
	
	if (m_pAsm != NULL)
	{
		ProMdlDisplay(m_pAsm);
		int nWndID;
		ProMdlWindowGet(m_pAsm, &nWndID);
		ProWindowActivate(nWndID);
	}
	CDialog::OnOK();
}

void CDlgSetThick::OnCancel() 
{
	if (MessageBox(L"确认退出当前操作？", L"提示", MB_OKCANCEL|MB_ICONINFORMATION) == IDOK)
	{
		if (m_pAsm != NULL)
		{
			ProMdlDisplay(m_pAsm);
			int nWndID;
			ProMdlWindowGet(m_pAsm, &nWndID);
			ProWindowActivate(nWndID);
		}
		CDialog::OnCancel();
	}
}

void CDlgSetThick::OnBnClickedSel()
{
	if (m_itemQuiltBase.owner == NULL)
	{
		return;
	}

	this->ShowWindow(SW_HIDE);
	vector<ProSelection> arrSelSurf;
	if (SelectObject(arrSelSurf, "qltface", MAX_SELECTION))
	{
		double dThickValue;
		CString strTips;
		if (m_nOffsetOrient > 0)
			strTips = L"指定当前面的加厚值，加厚方向为向外：";
		else
			strTips = L"指定当前面的加厚值，加厚方向为向内：";
		ShowMessageTip(strTips);

		if (ProMessageDoubleRead(NULL, &dThickValue) == PRO_TK_NO_ERROR)
		{
			ProQuilt quilt;
			ProGeomitemToQuilt(&m_itemQuiltBase, &quilt);

			for (int i=0; i<arrSelSurf.size(); i++)
			{
				ProModelitem itemSurf;
				ProSelectionModelitemGet(arrSelSurf[i], &itemSurf);
				ProSurface surf;
				ProGeomitemToSurface(&itemSurf, &surf);
				ProQuilt quiltOwner;
				ProSurfaceQuiltGet(ProMdlToSolid(m_itemQuiltBase.owner), surf, &quiltOwner);
				if (quiltOwner == quilt)
				{
					for (int j=0; j<m_arrQltfaceData.size(); j++)
					{
						if (surf == m_arrQltfaceData[j].qltface)
						{
							CString str;
							str.Format(L"曲面:%d (thick=%.2f)", m_arrQltfaceData[j].nSurfID, dThickValue);
							m_arrQltfaceData[j].dOffset = dThickValue;
							m_listSurf.DeleteString(j);
							m_listSurf.InsertString(j, str);
							m_listSurf.SetCurSel(j);
							break;
						}
					}
				}
			}
			if (dThickValue > m_dMaxThick)
				m_dMaxThick = dThickValue;
		}
	}
	this->ShowWindow(SW_SHOW);
}

void CDlgSetThick::InitSurfData(ProMdl pAsm, ProSelection selQuilt, double dThick)
{
	m_arrQltfaceData.clear();

	// 获取全部面
	m_pAsm = pAsm;
	ProSelectionModelitemGet(selQuilt, &m_itemQuiltBase);
	ProQuilt quilt;
	ProGeomitemToQuilt(&m_itemQuiltBase, &quilt);
	vector<ProSurface> arrQltface;
	ProQuiltSurfaceVisit(quilt, QuiltSurfacesGetAction, NULL, &arrQltface);

	if (dThick < 0.0)
		m_nOffsetOrient = -1;

	for (int i=0; i<arrQltface.size(); i++)
	{
		QltfaceData data;
		data.qltface = arrQltface[i];
		data.dOffset = fabs(dThick);
		ProSurfaceIdGet(arrQltface[i], &data.nSurfID);
		m_arrQltfaceData.push_back(data);
		m_mapQltfaceIndex.insert(make_pair(data.nSurfID, i));
	}
	m_dMaxThick = fabs(dThick);
}

void CDlgSetThick::OnLbnSelchangeListSurf()
{
	ProWindowRepaint(PRO_VALUE_UNUSED);
	ProSelbufferClear();

	int nIndex = m_listSurf.GetCurSel();
	if (nIndex >= 0 && m_itemQuiltBase.owner != NULL)
	{
		ProGeomitem itemSurf;
		ProSurfaceToGeomitem(ProMdlToSolid(m_itemQuiltBase.owner), m_arrQltfaceData[nIndex].qltface, &itemSurf);
		ProSelection selSurf;
		ProSelectionAlloc(NULL, &itemSurf, &selSurf);
		//ProSelectionHighlight(selSurf, PRO_COLOR_HIGHLITE);
		ProSelectionDisplay(selSurf);
	}
}

void CDlgSetThick::OnLbnDblclkListSurf()
{
	int nIndex = m_listSurf.GetCurSel();
	if (nIndex >= 0 && m_itemQuiltBase.owner != NULL)
	{
		double dThickValue;
		CString strTips;
		if (m_nOffsetOrient > 0)
			strTips = L"指定当前面的加厚值，加厚方向为向外：";
		else
			strTips = L"指定当前面的加厚值，加厚方向为向内：";
		ShowMessageTip(strTips);
		if (ProMessageDoubleRead(NULL, &dThickValue) == PRO_TK_NO_ERROR)
		{
			CString str;
			str.Format(L"曲面:%d (thick=%.2f)", m_arrQltfaceData[nIndex].nSurfID, dThickValue);
			m_arrQltfaceData[nIndex].dOffset = dThickValue;
			m_listSurf.DeleteString(nIndex);
			m_listSurf.InsertString(nIndex, str);
			if (dThickValue > m_dMaxThick)
				m_dMaxThick = dThickValue;
		}
	}
}

// 检查裁剪后的面与邻面是否继续相交
BOOL CDlgSetThick::CheckIntersectAfterTrim(QltfaceData& dataQuilt)
{
	BOOL bResult = TRUE;
	ProMdl pMdlPart = m_itemQuiltBase.owner;
	ProQuilt quilt;
	ProGeomitemToQuilt(&dataQuilt.itemOffsetQlt, &quilt);
	for (int i=0; i<dataQuilt.arrNeighborface.size(); i++)
	{
		int nNeighborIndexTemp = m_mapQltfaceIndex[dataQuilt.arrNeighborface[i]];
		ProQuilt quiltTemp;
		ProGeomitemToQuilt(&m_arrQltfaceData[nNeighborIndexTemp].itemOffsetQlt, &quiltTemp);

		if (!CheckTwoQuiltInstersect(pMdlPart, quilt, quiltTemp))
		{
			bResult = FALSE;
			break;
		}
	}
	return bResult;
}
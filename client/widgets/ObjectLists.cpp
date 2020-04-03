/*
 * ObjectLists.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "ObjectLists.h"

#include "../gui/CGuiHandler.h"
#include "Buttons.h"
#include "Images.h"
#include "TextControls.h"
#include "MiscWidgets.h"
#include "../windows/CWindowObject.h"

CObjectList::CObjectList(CreateFunc create)
	: createObject(create)
{
}

void CObjectList::deleteItem(std::shared_ptr<CIntObject> item)
{
	if(!item)
		return;
	item->deactivate();
	removeChild(item.get());
}

std::shared_ptr<CIntObject> CObjectList::createItem(size_t index)
{
	OBJECT_CONSTRUCTION_CUSTOM_CAPTURING(255-DISPOSE);
	std::shared_ptr<CIntObject> item = createObject(index);
	if(!item)
		item = std::make_shared<CIntObject>();

	item->recActions = defActions;
	addChild(item.get());
	item->activate();
	return item;
}

CTabbedInt::CTabbedInt(CreateFunc create, Point position, size_t ActiveID)
	: CObjectList(create),
	activeTab(nullptr),
	activeID(ActiveID)
{
	defActions &= ~DISPOSE;
	pos += position;
	reset();
}

void CTabbedInt::setActive(size_t which)
{
	if(which != activeID)
	{
		activeID = which;
		reset();
	}
}

void CTabbedInt::reset()
{
	deleteItem(activeTab);
	activeTab = createItem(activeID);
	activeTab->moveTo(pos.topLeft());

	if(active)
		redraw();
}

std::shared_ptr<CIntObject> CTabbedInt::getItem()
{
	return activeTab;
}

CListBox::CListBox(CreateFunc create, Point Pos, Point ItemOffset, size_t VisibleSize,
		size_t TotalSize, size_t InitialPos, int Slider, Rect SliderPos)
	: CObjectList(create),
	first(InitialPos),
	totalSize(TotalSize),
	itemOffset(ItemOffset)
{
	pos += Pos;
	items.resize(VisibleSize, nullptr);

	if(Slider & 1)
	{
		OBJECT_CONSTRUCTION_CAPTURING(255-DISPOSE);
		slider = std::make_shared<CSlider>(SliderPos.topLeft(), SliderPos.w, std::bind(&CListBox::moveToPos, this, _1),
			VisibleSize, TotalSize, InitialPos, Slider & 2, Slider & 4 ? CSlider::BLUE : CSlider::BROWN);
	}
	reset();
}

// Used to move active items after changing list position
void CListBox::updatePositions()
{
	Point itemPos = pos.topLeft();
	for (auto & elem : items)
	{
		(elem)->moveTo(itemPos);
		itemPos += itemOffset;
	}
	if (active)
	{
		redraw();
		if (slider)
			slider->moveTo(first);
	}
}

void CListBox::reset()
{
	size_t current = first;
	for (auto & elem : items)
	{
		deleteItem(elem);
		elem = createItem(current++);
	}
	updatePositions();
}

void CListBox::resize(size_t newSize)
{
	totalSize = newSize;
	if (slider)
		slider->setAmount(totalSize);
	reset();
}

size_t CListBox::size()
{
	return totalSize;
}

std::shared_ptr<CIntObject> CListBox::getItem(size_t which)
{
	if(which < first || which > first + items.size() || which > totalSize)
		return std::shared_ptr<CIntObject>();

	size_t i=first;
	for (auto iter = items.begin(); iter != items.end(); iter++, i++)
		if( i == which)
			return *iter;
	return std::shared_ptr<CIntObject>();
}

size_t CListBox::getIndexOf(std::shared_ptr<CIntObject> item)
{
	size_t i=first;
	for(auto iter = items.begin(); iter != items.end(); iter++, i++)
		if(*iter == item)
			return i;
	return size_t(-1);
}

void CListBox::scrollTo(size_t which)
{
	//scroll up
	if(first > which)
		moveToPos(which);
	//scroll down
	else if (first + items.size() <= which && which < totalSize)
		moveToPos(which - items.size() + 1);
}

void CListBox::moveToPos(size_t which)
{
	//Calculate new position
	size_t maxPossible;
	if(totalSize > items.size())
		maxPossible = totalSize - items.size();
	else
		maxPossible = 0;

	size_t newPos = std::min(which, maxPossible);

	//If move distance is 1 (most of calls from Slider) - use faster shifts instead of resetting all items
	if(first - newPos == 1)
	{
		moveToPrev();
	}
	else if(newPos - first == 1)
	{
		moveToNext();
	}
	else if(newPos != first)
	{
		first = newPos;
		reset();
	}
}

void CListBox::moveToNext()
{
	//Remove front item and insert new one to end
	if(first + items.size() < totalSize)
	{
		first++;
		deleteItem(items.front());
		items.pop_front();
		items.push_back(createItem(first+items.size()));
		updatePositions();
	}
}

void CListBox::moveToPrev()
{
	//Remove last item and insert new one at start
	if(first)
	{
		first--;
		deleteItem(items.back());
		items.pop_back();
		items.push_front(createItem(first));
		updatePositions();
	}
}

size_t CListBox::getPos()
{
	return first;
}

const std::list<std::shared_ptr<CIntObject>> & CListBox::getItems()
{
	return items;
}

enum {DropBoxLabelOffset = 3};

class DropBoxList: public CWindowObject
{
	CDropBox *owner;
	std::vector<std::string> itemNames;
	std::vector<std::unique_ptr<CLabel>> labels;
	std::unique_ptr<CSlider> slider;
	int mouseX, mouseY;
	unsigned visibleItems;
	unsigned sliderPosition;

	void setPosition(unsigned value);
public:
	DropBoxList(CDropBox *owner, const std::string &backgroundImage, int x, int y,
	            const std::vector<std::string> items, unsigned selectedIndex,
	            unsigned visibleItems);
	void clickLeft(tribool down, bool previousState) override;
	void mouseMoved (const SDL_MouseMotionEvent & sEvent) override;
};

DropBoxList::DropBoxList(CDropBox *owner, const std::string &backgroundImage, int x, int y,
                         const std::vector<std::string> items, unsigned selectedIndex,
                         unsigned visibleItems)
: CWindowObject(CWindowObject::EOptions::SHADOW_DISABLED, backgroundImage),
  owner(owner),
  itemNames(items),
  visibleItems(visibleItems)
{
	OBJ_CONSTRUCTION_CAPTURING_ALL_NO_DISPOSE;
	moveBy(Point(x-pos.x, y-pos.y), true);

	unsigned startingPosition;
	if ((selectedIndex < visibleItems/2) || (visibleItems >= items.size()))
		startingPosition = 0;
	else
	{
		startingPosition = selectedIndex - visibleItems/2;
		if (startingPosition + visibleItems > items.size())
			startingPosition = items.size() - visibleItems;
	}

	slider = std::unique_ptr<CSlider>(new CSlider(Point(0, 0), pos.h, [this](int pos) { setPosition(pos); redraw(); },
	                                              visibleItems, items.size(), startingPosition, false, CSlider::BLUE));
	static_cast<CIntObject *>(slider.get())->moveBy(Point(pos.w - slider->pos.w, 0));

	labels.resize(std::min((unsigned)items.size(), visibleItems));
	setPosition(startingPosition);
	addUsedEvents(LCLICK | MOVE);
}

void DropBoxList::mouseMoved (const SDL_MouseMotionEvent & sEvent)
{
	mouseX = sEvent.x - pos.x;
	mouseY = sEvent.y - pos.y;
}

void DropBoxList::clickLeft(tribool down, bool previousState)
{
	if (down && (mouseX < pos.w - slider->pos.w))
	{
		int y = std::max(mouseY, 0);
		unsigned index = sliderPosition + (y * visibleItems) / pos.h;
		if (index < itemNames.size())
		{
			owner->selectedIndex = index;
			owner->selection->setText(itemNames[index]);
			if (owner->selectionCallback)
				owner->selectionCallback(index);
		}
		close();
	}
}

void DropBoxList::setPosition(unsigned position)
{
	OBJ_CONSTRUCTION_CAPTURING_ALL_NO_DISPOSE;

	sliderPosition = position;
	for (unsigned i = 0; i < visibleItems; i++)
	{
		unsigned labelIndex = position+i;
		if (labelIndex >= itemNames.size())
			break;

		labels[i] = std::unique_ptr<CLabel>(new CLabel(Rect(DropBoxLabelOffset, 0, pos.w - DropBoxLabelOffset - slider->pos.w, 0),
		                                               FONT_SMALL, TOPLEFT, Colors::WHITE, itemNames[labelIndex]));
		labels[i]->moveBy(Point(0, (pos.h * i)/visibleItems + (pos.h/visibleItems - labels[i]->pos.h)/2));
	}
}

CDropBox::CDropBox(Point topLeft, const std::string &selectionBgImage,
                   const std::string &listBgImage, unsigned listVisibleSize,
                   EFonts listFont, const std::vector<std::string> &items,
                   unsigned selectedIndex)
: selectionCallback(nullptr)
{
	selectionBg = std::unique_ptr<CPicture>(new CPicture(selectionBgImage, topLeft.x, topLeft.y));
	pos = selectionBg->pos;

	selection = std::unique_ptr<CLabel>(new CLabel(Rect(topLeft.x + DropBoxLabelOffset, topLeft.y, pos.w - DropBoxLabelOffset - 20, 0),
	                                               listFont, TOPLEFT, Colors::WHITE,
	                                               (selectedIndex < items.size()) ? items[selectedIndex] : ""));
	itemNames = items;
	this->listVisibleSize = listVisibleSize;
	this->selectedIndex = selectedIndex;
	this->listBackgroundName = listBgImage;
	addUsedEvents(LCLICK);
}

CDropBox::~CDropBox()
{
}

void CDropBox::clickLeft(tribool down, bool previousState)
{
	if (down)
		GH.pushIntT<DropBoxList>(this, listBackgroundName, pos.x, pos.y, itemNames, selectedIndex, listVisibleSize);
}

void CDropBox::setSelectionCallback(const std::function<void(unsigned)> &callback)
{
	selectionCallback = callback;
}

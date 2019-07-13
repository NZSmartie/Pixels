﻿using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.UI;

public class ColorAnimator : MonoBehaviour, IFocusable
{
    [SerializeField]
    Image _image = null;
    [SerializeField]
	Text _number = null;
	[SerializeField]
	MovableArea _movableColorSlider = null;
	[SerializeField]
	RectTransform _confirmRemovePanel = null;

	public event System.Action<ColorAnimator> GotFocus;
	public bool HasFocus { get; private set; }

	public float LeftBound { get { return _movableColorSlider.LeftBound; } set { _movableColorSlider.LeftBound = value; } }
	public float RightBound { get { return _movableColorSlider.RightBound; } set { _movableColorSlider.RightBound = value; } }

	public MultiSlider ColorSlider => _movableColorSlider.Movable.GetComponentInChildren<MultiSlider>();

    public int LedNumber => int.Parse(_number.text);

	public void ChangeLed()
	{
		LedSelectorPanel.Instance.Show(number =>
		{
            if (number != -1)
            {
                SetLedNumber(number);
            }
        });
	}

	public void SetLedNumber(int number)
	{
		_number.text = (number + 1).ToString();
	}

	public void ShowColor(float cursorPos)
	{
		_image.color = ColorSlider.GetColorAt(cursorPos);
	}

	public void ResetColor()
	{
		_image.color = Color.white;
	}

	public void Maximize()
	{
		_movableColorSlider.Maximize();
	}

	public void ConfirmRemoveSelf()
	{
		ColorSlider.SelectHandle(null);
		ShowConfirmRemove();
	}

	public void RemoveSelf()
	{
		var timeline = GetComponentInParent<TimelineView>();
		gameObject.transform.SetParent(null);
		GameObject.Destroy(gameObject);
		//TODO ActiveColorAnimator = null;
		timeline.Repaint(); //TODO
	}

	public void DuplicateSelf()
	{
		RemoveFocus();
		var timeline = GetComponentInParent<TimelineView>();
		timeline.AddTrack(ToAnimationTrack(timeline.Unit));
	}

	public void GiveFocus()
	{
		if (!HasFocus)
		{
			HasFocus = true;
			if (GotFocus != null) GotFocus(this);
		}
	}

	public void RemoveFocus()
	{
		HasFocus = false;
		foreach (var focusable in FindFocusables())
		{
			focusable.RemoveFocus();
		}
	}

    public Animations.EditTrack ToAnimationTrack(float unitSize)
    {
        var rect = (ColorSlider.transform as RectTransform).rect;
        return new Animations.EditTrack()
        {
            ledIndex = LedNumber - 1,
            keyframes = ColorSlider.ToAnimationKeyFrames(unitSize),
        };
    }

    public void FromAnimationTrack(Animations.EditTrack track, float unitSize)
    {
        ShowConfirmRemove(false);
        SetLedNumber(track.ledIndex);
        LeftBound = track.firstTime * unitSize;
        RightBound = track.lastTime * unitSize;
        ColorSlider.FromAnimationKeyframes(track.keyframes, unitSize);
    }

    // public static int LedSpriteToIndex(string spriteName)
	// {
	// 	int face = int.Parse(spriteName[spriteName.IndexOf('-') - 1].ToString());
	// 	int point = int.Parse(spriteName[spriteName.IndexOf('-') + 1].ToString());
	// 	return Enumerable.Range(1, face - 1).Sum() + point - 1;
	// }

	// public static string IndexToLedSprite(int index)
	// {
	// 	int face = 1;
	// 	int count = 0;	
	// 	while ((count + face) <= index)
	// 	{
	// 		count += face;
	// 		++face;
	// 	}
	// 	int point = 1 + index - count;
	// 	return string.Format("Led{0}-{1}", face, point);
	// }

	IFocusable[] FindFocusables()
	{
		return GetComponentsInChildren<IFocusable>().Where(f => (object)f != this).ToArray();
	}

	void ShowConfirmRemove(bool show = true)
	{
		_confirmRemovePanel.gameObject.SetActive(show);
	}

	// Use this for initialization
	void Start()
	{
		ShowConfirmRemove(false);
	}
}

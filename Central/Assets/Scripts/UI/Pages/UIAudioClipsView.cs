﻿using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using AudioClips;
using System.Text;
using SimpleFileBrowser;

public class UIAudioClipsView
    : UIPage
{
    [Header("Controls")]
    public Transform contentRoot;
    public Button addAudioClipButton;
    public RectTransform spacer;

    [Header("Prefabs")]
    public UIAudioClipsViewToken audioClipTokenPrefab;

    List<UIAudioClipsViewToken> audioClips = new List<UIAudioClipsViewToken>();

    AudioSource audioSource;
    string currentFilepath;

    void Awake()
    {
        audioSource = GetComponent<AudioSource>();
        addAudioClipButton.onClick.AddListener(AddNewClip);
    }

    void OnEnable()
    {
        base.SetupHeader(true, false, "Audio Clips", null);
        RefreshView();
    }

    UIAudioClipsViewToken CreateClipToken(AudioClipManager.AudioClipInfo clip)
    {
        // Create the gameObject
        var ret = GameObject.Instantiate<UIAudioClipsViewToken>(audioClipTokenPrefab, Vector3.zero, Quaternion.identity, contentRoot.transform);
        spacer.SetAsLastSibling();

        // When we click on the pattern main button, go to the edit page
        ret.onPlay.AddListener(() => PlayClip(clip));
        ret.onRemove.AddListener(() => DeleteClip(clip));
        ret.onExpand.AddListener(() => ExpandClip(clip));
        ret.onClick.AddListener(() => ExpandClip(clip));

        addAudioClipButton.transform.SetAsLastSibling();
        // Initialize it
        ret.Setup(clip);
        return ret;
    }

    void DestroyClipToken(UIAudioClipsViewToken clipToken)
    {
        GameObject.Destroy(clipToken.gameObject);
    }

    void RefreshView()
    {
        // Assume all pool dice will be destroyed
        List<UIAudioClipsViewToken> toDestroy = new List<UIAudioClipsViewToken>(audioClips);
        foreach (var clip in AudioClipManager.Instance.audioClips)
        {
            int prevIndex = toDestroy.FindIndex(a => a.clip == clip);
            if (prevIndex == -1)
            {
                // New clip
                var newClipUI = CreateClipToken(clip);
                audioClips.Add(newClipUI);
            }
            else
            {
                toDestroy.RemoveAt(prevIndex);
            }
        }

        // Remove all remaining behaviors
        foreach (var bh in toDestroy)
        {
            audioClips.Remove(bh);
            DestroyClipToken(bh);
        }
    }

    IEnumerator FileSelectedCr(string filePath)
    {
        Debug.Log("Audio file path: " + filePath);
        // Copy the file to the user directory
        string fileName = null;
        yield return AudioClipManager.Instance.AddUserClip(filePath, n => fileName = n);
        if (!string.IsNullOrEmpty(fileName))
        {
            AppDataSet.Instance.AddAudioClip(filePath);
            RefreshView();
        }
    }

    void FileSelected(string filePath)
    {
        StartCoroutine(FileSelectedCr(filePath));
    }

    void AddNewClip()
    {
#if UNITY_EDITOR
        FileSelected(UnityEditor.EditorUtility.OpenFilePanel("Select audio file", "", "wav"));
#elif UNITY_STANDALONE_WIN
        // Set filters (optional)
		// It is sufficient to set the filters just once (instead of each time before showing the file browser dialog), 
		// if all the dialogs will be using the same filters
		FileBrowser.SetFilters( true, new FileBrowser.Filter( "Audio files", ".wav" ));

		// Set default filter that is selected when the dialog is shown (optional)
		// Returns true if the default filter is set successfully
		// In this case, set Images filter as the default filter
		FileBrowser.SetDefaultFilter( ".wav" );
        FileBrowser.ShowLoadDialog((paths) => FileSelected(paths[0]), null, FileBrowser.PickMode.Files, false, null, null, "Select audio file", "Select");
#else
        NativeFilePicker.PickFile( FileSelected, new string[] { NativeFilePicker.ConvertExtensionToFileType( "wav" ), NativeFilePicker.ConvertExtensionToFileType( "mp3" ) });
#endif
    }

    void DeleteClip(AudioClipManager.AudioClipInfo clip)
    {
        PixelsApp.Instance.ShowDialogBox("Delete AudioClip?", "Are you sure you want to delete " + clip.clip.name + "?", "Ok", "Cancel", res =>
        {
            if (res)
            {
                // var dependentActions = AppDataSet.Instance.CollectActionsForAudioClip(clip);
                // if (dependentActions.Any())
                // {
                //     StringBuilder builder = new StringBuilder();
                //     builder.Append("The following presets depend on ");
                //     builder.Append(behavior.name);
                //     builder.AppendLine(":");
                //     foreach (var b in dependentPresets)
                //     {
                //         builder.Append("\t");
                //         builder.AppendLine(b.name);
                //     }
                //     builder.Append("Are you sure you want to delete it?");

                //     PixelsApp.Instance.ShowDialogBox("Behavior In Use!", builder.ToString(), "Ok", "Cancel", res2 =>
                //     {
                //         if (res2)
                //         {
                //             AppDataSet.Instance.DeleteBehavior(behavior);
                //             AppDataSet.Instance.SaveData();
                //             RefreshView();
                //         }
                //     });
                // }
                // else
                // {
                //     AppDataSet.Instance.DeleteBehavior(behavior);
                //     AppDataSet.Instance.SaveData();
                //     RefreshView();
                // }
            }
        });
    }

    void ExpandClip(AudioClipManager.AudioClipInfo clip)
    {
        foreach (var uip in audioClips)
        {
            if (uip.clip == clip)
            {
                uip.Expand(!uip.isExpanded);
            }
            else
            {
                uip.Expand(false);
            }
        }
    }

    void PlayClip(AudioClipManager.AudioClipInfo clip)
    {
        audioSource.PlayOneShot(clip.clip);
    }
}

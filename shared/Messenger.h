#pragma once

#include <any>
#include <functional>

namespace Messenger
{
	/**
	 * \brief Types of messages
	 */
	enum class Message
	{
		/**
		 * \brief The emulator launched state has changed
		 */
		EmuLaunchedChanged,

		/**
		 * \brief The emulator is beginning the termination process
		 */
		EmuStopping,

		/**
		 * \brief The emulator paused state has changed
		 */
		EmuPausedChanged,

		/**
		 * \brief The video capturing state has changed
		 */
		CapturingChanged,

		/**
		 * \brief The statusbar visibility has changed
		 */
		StatusbarVisibilityChanged,

		/**
		 * \brief The main window size changed
		 */
		SizeChanged,

		/**
		 * \brief The movie loop state changed
		 */
		MovieLoopChanged,

		/**
		 * \brief The VCR read-only state changed
		 */
		ReadonlyChanged,

		/**
		 * \brief The VCR task changed
		 */
		TaskChanged,

		/**
		 * \brief The Lua engine has started a script
		 */
		ScriptStarted,

		/**
		 * \brief The main window has been created
		 */
		AppReady,

		/**
		 * \brief The emulator has finished resetting
		 */
		ResetCompleted,

		/**
		 * \brief The user has requested a reset. The reset won't be performed after the message is handled.
		 */
		ResetRequested,

		/**
		 * \brief The config will begin saving soon
		 */
		ConfigSaving,

		/**
		 * \brief The config has been loaded and values have changed
		 */
		ConfigLoaded,

		/**
		 * \brief The rerecord count of the currently recorded movie changed
		 */
		RerecordsChanged,

		/**
		 * \brief The currently selected save slot changed
		 */
		SlotChanged,

		/**
		 * \brief A VCR seek operation has completed
		 */
		SeekCompleted,

		/**
		 * \brief The core speed modifier has changed
		 */
		SpeedModifierChanged,

		/**
		 * \brief The threhsold of VIs since the last input poll has been exceeded
		 */
		LagLimitExceeded,

		/**
		 * \brief The emu has begun or stopped its starting process
		 */
		EmuStartingChanged,

		/**
		 * \brief The core is reporting on the status of an operation
		 */
		CoreResult,

		/**
		 * \brief The fullscreen mode has changed
		 */
		FullscreenChanged,

		/**
		 * \brief The audio dacrate has changed
		 */
		DacrateChanged,
	};

	/**
	 * \brief Initializes the messenger
	 */
	void init();

	/**
	 * \brief Broadcasts a message to all listeners
	 * \param message The message type
	 * \param data The message data
	 * \remark This method is thread-safe, but mustn't be called concurrently with subscribe()
	 */
	void broadcast(Message message, std::any data);

	/**
	 * \brief Subscribe to a message
	 * \param message The message type to listen for
	 * \param callback The callback to be invoked upon receiving the specified message type
	 * \return A function which, when called, unsubscribes from the message
	 * \remark This method is not thread-safe.
	 */
	std::function<void()> subscribe(Message message, std::function<void(std::any)> callback);
}

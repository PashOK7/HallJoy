#pragma once

#include "base.hpp"

#if SOUP_WINDOWS
#include <windows.h>
#else
#include <unistd.h> // close
#endif

NAMESPACE_SOUP
{
#if SOUP_WINDOWS
	struct HandleRaii
	{
		HANDLE h = INVALID_HANDLE_VALUE;

		HandleRaii() noexcept = default;

		HandleRaii(HANDLE h) noexcept
			: h(h)
		{
		}

		HandleRaii(HandleRaii&& b) noexcept
			: h(b.h)
		{
			b.h = INVALID_HANDLE_VALUE;
		}

		~HandleRaii() noexcept
		{
			if (isValid())
			{
				CloseHandle(h);
			}
		}

		void operator=(HANDLE new_handle) noexcept
		{
			if (h != new_handle)
			{
				if (isValid())
				{
					CloseHandle(h);
				}
				h = new_handle;
			}
		}
		void operator=(HandleRaii&& b) noexcept
		{
			if (this != &b)
			{
				if (isValid())
				{
					CloseHandle(h);
				}
				h = b.h;
				b.h = INVALID_HANDLE_VALUE;
			}
		}
		[[nodiscard]] operator bool() const noexcept
		{
			return isValid();
		}

		[[nodiscard]] operator HANDLE() const noexcept
		{
			return h;
		}

		[[nodiscard]] bool isValid() const noexcept
		{
			return h != INVALID_HANDLE_VALUE && h != NULL;
		}

		void invalidate() noexcept
		{
			h = INVALID_HANDLE_VALUE;
		}
	};
#else
	struct HandleRaii
	{
		int handle = -1;

		HandleRaii() noexcept = default;

		HandleRaii(int handle) noexcept
			: handle(handle)
		{
		}

		HandleRaii(HandleRaii&& b) noexcept
			: handle(b.handle)
		{
			b.handle = -1;
		}

		~HandleRaii() noexcept
		{
			if (isValid())
			{
				::close(handle);
			}
		}

		[[nodiscard]] bool isValid() const noexcept
		{
			return handle >= 0;
		}

		void operator=(int new_handle) noexcept
		{
			if (handle != new_handle)
			{
				if (isValid())
				{
					::close(handle);
				}
				handle = new_handle;
			}
		}
		void operator=(HandleRaii&& b) noexcept
		{
			if (this != &b)
			{
				if (isValid())
				{
					::close(handle);
				}
				handle = b.handle;
				b.handle = -1;
			}
		}
		operator int() const noexcept
		{
			return handle;
		}
	};
#endif
}

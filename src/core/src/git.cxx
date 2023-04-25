#include "git.hxx"

namespace valet
{

bool prepare_git_dep(const git_info& info, std::filesystem::path& out_folder)
{
	// 1. Clone the repo to user's cache folder
	// 2. Checkout the branch/rev/tag
	// 3. Make sure the folder contains a valet manifest.
	// 4. Save the folder path to some place private to current package that is being built, 
	//    so that it doesn't get fetched every time unless a 'valet update' is called.
	// 5. Return the folder path to the caller.
	return true;
}

} // namespace valet
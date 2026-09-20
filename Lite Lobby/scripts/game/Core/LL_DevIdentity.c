// Identity for a development server without backend reach, where players audit with an
// empty identity. Same derivation the game applies in a non-dedicated session, so the id
// matches Workbench play. Switched on in $profile:LL_PlayerVerification.json.

class LL_DevIdentity
{
	static string FromName(string playerName)
	{
		if (playerName == "")
			return "";

		int splitLength = Math.Max(1, playerName.Length() / 3);
		string split1 = Math.AbsInt(playerName.Substring(0, splitLength).Hash()).ToString(8, true);
		string split2 = Math.AbsInt(playerName.Substring(splitLength, splitLength).Hash()).ToString(8, true);
		int doubleSplit = splitLength * 2;
		string split3 = Math.AbsInt(playerName.Substring(doubleSplit, playerName.Length() - doubleSplit).Hash()).ToString(8, true);

		string uid = string.Format("00bbbddd-%1-%2-%3-%4%5", split1.Substring(0, 4), split1.Substring(4, 4), split2.Substring(0, 4), split2.Substring(4, 4), split3);
		uid.ToLower();
		return uid;
	}
}

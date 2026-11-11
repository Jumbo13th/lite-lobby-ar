//! Run from Resource Manager > Plugins, or with -wbModule=ResourceManager -plugin=LL_MapPaintTests.
[WorkbenchPluginAttribute(name: "Validate Map Paint", category: "Lite Lobby", wbModules: { "ResourceManager" })]
class LL_MapPaintTests : WorkbenchPlugin
{
	protected int m_iChecks;
	protected int m_iFailures;

	protected void Check(bool condition, string name)
	{
		m_iChecks++;
		if (condition)
			return;
		m_iFailures++;
		Print("LL_MAP_PAINT FAIL: " + name, LogLevel.ERROR);
	}

	protected bool Near(float actual, float expected)
	{
		return Math.AbsFloat(actual - expected) < 0.001;
	}

	protected LL_MapPaintDocument MakeLine()
	{
		LL_MapPaintDocument document = new LL_MapPaintDocument();
		LL_MapPaintStroke stroke = document.BeginStroke(Vector(-10, 0, 0), 2, 0xffc54b4b);
		document.AppendPoint(stroke, Vector(10, 0, 0), 0);
		return document;
	}

	protected void Validate()
	{
		m_iChecks = 0;
		m_iFailures = 0;
		// Compilation alone cannot detect loss of BaseContainerProps on a modded class.
		Resource mapResource = BaseContainerTools.LoadContainer("{1B8AC767E06A0ACD}Configs/Map/MapFullscreen.conf");
		SCR_MapConfig mapConfig;
		if (mapResource && mapResource.IsValid())
			mapConfig = SCR_MapConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(mapResource.GetResource().ToBaseContainer()));
		bool hasCursor;
		if (mapConfig && mapConfig.m_aModules)
		{
			foreach (SCR_MapModuleBase module : mapConfig.m_aModules)
			{
				if (SCR_MapCursorModule.Cast(module))
					hasCursor = true;
			}
		}
		Check(hasCursor, "vanilla map config deserializes its cursor module");
		float first, last;
		bool hit = LL_MapPaintGeometry.CircleInterval(Vector(-10, 0, 0), Vector(10, 0, 0), vector.Zero, 2, first, last);
		Check(hit && Near(first, 0.4) && Near(last, 0.6), "circle crossing interval");
		hit = LL_MapPaintGeometry.CircleInterval(Vector(-10, 0, 5), Vector(10, 0, 5), vector.Zero, 2, first, last);
		Check(!hit, "circle miss");
		hit = LL_MapPaintGeometry.CapsuleInterval(Vector(-10, 0, 0), Vector(10, 0, 0), Vector(0, 0, -100), Vector(0, 0, 100), 2, first, last);
		Check(hit && Near(first, 0.4) && Near(last, 0.6), "fast eraser sweep crosses stroke between endpoints");
		hit = LL_MapPaintGeometry.CapsuleInterval(Vector(-10, 0, 0), Vector(10, 0, 0), Vector(-4, 0, 0), Vector(4, 0, 0), 2, first, last);
		Check(hit && Near(first, 0.2) && Near(last, 0.8), "parallel sweep includes end caps");
		hit = LL_MapPaintGeometry.CapsuleInterval(vector.Zero, vector.Zero, Vector(-10, 0, 0), Vector(10, 0, 0), 2, first, last);
		Check(hit, "swept eraser hits a single dot");

		LL_MapPaintDocument document = MakeLine();
		Check(!document.Erase(Vector(0, 0, 20), Vector(0, 0, 30), 2), "no-op erasing reports no change");
		Check(document.m_Strokes.Count() == 1 && document.m_Strokes[0].m_Points.Count() == 2, "no-op erasing preserves stroke");
		Check(document.Erase(vector.Zero, vector.Zero, 2), "partial erasing reports change");
		Check(document.m_Strokes.Count() == 2, "partial erasing keeps both sides");
		if (document.m_Strokes.Count() == 2)
		{
			Check(Near(document.m_Strokes[0].m_Points[1][0], -3), "left cut includes pen radius");
			Check(Near(document.m_Strokes[1].m_Points[0][0], 3), "right cut includes pen radius");
			Check(document.m_Strokes[1].m_iColor == 0xffc54b4b && Near(document.m_Strokes[1].m_fWidth, 2), "fragments preserve pen style");
		}
		document.Erase(Vector(-20, 0, 0), Vector(20, 0, 0), 2);
		Check(document.m_Strokes.IsEmpty(), "full sweep removes surviving fragments");

		document = new LL_MapPaintDocument();
		document.BeginStroke(Vector(5, 0, 5), 2, 0xffc54b4b);
		Check(document.Erase(Vector(5, 0, 5), Vector(5, 0, 5), 1) && document.m_Strokes.IsEmpty(), "erase click removes a dot");

		document = new LL_MapPaintDocument();
		LL_MapPaintStroke stroke = document.BeginStroke(Vector(-10, 0, 0), 2, 0xffc54b4b);
		document.AppendPoint(stroke, vector.Zero, 0);
		document.AppendPoint(stroke, Vector(10, 0, 0), 0);
		document.Erase(vector.Zero, vector.Zero, 2);
		Check(document.m_Strokes.Count() == 2, "erasing a shared vertex leaves two disconnected fragments");
		Check(!document.AppendPoint(document.m_Strokes[0], Vector(-9, 0, 0), 100), "sampling ignores sub-threshold motion");

		document = new LL_MapPaintDocument();
		stroke = document.BeginStroke(vector.Zero, 2, 0xffc54b4b);
		for (int i = 1; i < LL_MapPaintDocument.MAX_POINTS; i++)
			document.AppendPoint(stroke, Vector(i, 0, 0), 0);
		Check(!document.AppendPoint(stroke, Vector(9000, 0, 0), 0), "point budget stops growing an existing stroke");
		Check(!document.BeginStroke(vector.Zero, 2, 0xffc54b4b), "point budget stops creating new strokes");
		document.Erase(Vector(4096, 0, 0), Vector(4096, 0, 0), 10000);
		Check(document.BeginStroke(vector.Zero, 2, 0xffc54b4b) != null, "erasing releases the point budget");

		array<float> vertices = {};
		for (int v = 0; v < 1000; v++)
			vertices.Insert(v);
		array<ref CanvasWidgetCommand> commands = {};
		LL_MapPaintCanvas.AddLines(vertices, 4, 0xffc54b4b, commands);
		Check(commands.Count() == 3, "long strokes split into bounded canvas commands");
		bool bounded = true;
		bool connected = true;
		LineDrawCommand previous;
		foreach (CanvasWidgetCommand command : commands)
		{
			LineDrawCommand line = LineDrawCommand.Cast(command);
			bounded &= line.m_Vertices.Count() <= SCR_MapConstants.CANVAS_COMMAND_VERTICES_LIMIT;
			if (previous)
			{
				int previousCount = previous.m_Vertices.Count();
				connected &= Near(previous.m_Vertices[previousCount - 2], line.m_Vertices[0]);
				connected &= Near(previous.m_Vertices[previousCount - 1], line.m_Vertices[1]);
			}
			previous = line;
		}
		Check(bounded, "every canvas command obeys the engine vertex limit");
		Check(connected, "chunk boundaries share endpoints without gaps");
		ValidateSpatialErasing();
		ValidateIncrementalIndex();
		ValidateReplication();
		ValidateCircles();
		ValidateMarkerReplay();
		ValidateHistory();
		ValidateEraseHistory();
		ValidateHistoryBatches();
		ValidateHistoryWindow();
		ValidateMapWindowPlacement();

		Print(string.Format("LL_MAP_PAINT RESULT: %1 checks, %2 failures", m_iChecks, m_iFailures));
	}

	protected bool SameDrawing(LL_MapPaintDocument actual, LL_MapPaintDocument expected)
	{
		if (actual.GetPointCount() != expected.GetPointCount() || actual.m_Strokes.Count() != expected.m_Strokes.Count())
			return false;
		foreach (int index, LL_MapPaintStroke stroke : actual.m_Strokes)
		{
			LL_MapPaintStroke other = expected.m_Strokes[index];
			if (stroke.m_iColor != other.m_iColor || stroke.m_iOwner != other.m_iOwner
				|| stroke.m_bAnchorLine != other.m_bAnchorLine || !Near(stroke.m_fWidth, other.m_fWidth)
				|| stroke.m_Points.Count() != other.m_Points.Count())
				return false;
			foreach (int pointIndex, vector point : stroke.m_Points)
			{
				if (vector.DistanceSq(point, other.m_Points[pointIndex]) > 0.000001)
					return false;
			}
		}
		return true;
	}

	protected void ValidateSpatialErasing()
	{
		LL_MapPaintDocument document = MakeLine();
		array<ref LL_MapPaintSpatialSegment> nearby = {};
		array<vector> path = {Vector(0, 0, -1000000), Vector(0, 0, 1000000)};
		document.QuerySpatialPath(path, 2, nearby);
		Check(!nearby.IsEmpty(), "documents index strokes without opening a map");
		Check(document.ErasePath(path, 2) && document.m_Strokes.Count() == 2, "indexed sweep finds crossings far from both path endpoints");

		document = new LL_MapPaintDocument();
		document.ConfigureSpatialGrid(Vector(-500, 0, -500), Vector(1000, 0, 1000));
		array<vector> nearLine = {Vector(-10, 0, 0), Vector(10, 0, 0), Vector(10, 0, 400), Vector(400, 0, 400)};
		document.AddStroke(nearLine, 2, 0xffc54b4b, 7);
		array<vector> farLine = {Vector(400, 0, -400), Vector(450, 0, -400)};
		LL_MapPaintStroke untouched = document.AddStroke(farLine, 2, 0xff00ff00, 8);
		path = {vector.Zero};
		document.QuerySpatialPath(path, 2, nearby);
		bool localOnly = nearby.Count() == 2;
		foreach (LL_MapPaintSpatialSegment segment : nearby)
			localOnly &= segment.m_Stroke != untouched && segment.m_iSegmentIndex != 3;
		Check(localOnly, "spatial query excludes distant strokes and distant segments of a nearby stroke");
		Check(document.ErasePath(path, 2) && document.m_Strokes.Contains(untouched), "erasing preserves untouched stroke objects and drawing order");
		document.QuerySpatialPath(path, 2, nearby);
		Check(!document.ErasePath(path, 1), "rebuilt index does not retain removed geometry");
		array<vector> outside = {Vector(-1000, 0, -1000)};
		document.BeginStroke(outside[0], 2, 0xffc54b4b);
		Check(document.ErasePath(outside, 1), "indexed erase supports negative cells outside terrain bounds");
		document = new LL_MapPaintDocument();
		document.ConfigureSpatialGrid(vector.Zero, Vector(1000, 0, 1000));
		array<vector> wideLine = {Vector(-100, 0, 30), Vector(100, 0, 30)};
		document.AddStroke(wideLine, 60, 0xffc54b4b, 7);
		document.QuerySpatialPath(path, 1, nearby);
		Check(nearby.Count() == 1 && document.ErasePath(path, 1), "query includes a wide stroke whose centerline lies outside the eraser cells");
		array<vector> boundary = {Vector(20, 0, 20)};
		document.AddStroke(boundary, 2, 0xffc54b4b);
		Check(document.ErasePath(boundary, 1), "erase click finds a dot exactly on a cell boundary");

		// Compare different grid bounds with an unfiltered reference. Include wide
		// lines, dots, turns, overlapping sweeps, repeated samples and later appends.
		LL_MapPaintDocument baseline = new LL_MapPaintFullScanDocument();
		document = new LL_MapPaintDocument();
		document.ConfigureSpatialGrid(Vector(-300, 0, -700), Vector(1700, 0, 2300));
		for (int s = 0; s < 40; s++)
		{
			array<vector> points = {};
			int pointCount = 1 + s % 6;
			for (int p = 0; p < pointCount; p++)
			{
				int x = -600 + (s * 137 + p * 251) % 1600;
				int z = -800 + (s * 263 + p * 173) % 1800;
				points.Insert(Vector(x, 0, z));
			}
			int width = 2 + s % 11 * 10;
			document.AddStroke(points, width, 0xffc54b4b, s % 4, s % 3 == 0);
			baseline.AddStroke(points, width, 0xffc54b4b, s % 4, s % 3 == 0);
		}
		for (int sweep = 0; sweep < 24; sweep++)
		{
			path = {};
			for (int sample = 0; sample < 1 + sweep % 4; sample++)
			{
				int x = -700 + (sweep * 191 + sample * 389) % 1900;
				int z = -900 + (sweep * 311 + sample * 227) % 2100;
				path.Insert(Vector(x, 0, z));
			}
			if (sweep % 5 == 0)
				path.Insert(path[0]);
			int radius = 1 + sweep % 7 * 9;
			bool actualChanged = document.ErasePath(path, radius);
			bool expectedChanged = baseline.ErasePath(path, radius);
			Check(actualChanged == expectedChanged && SameDrawing(document, baseline), "indexed erasing matches full scan, sweep " + sweep.ToString());
			if (sweep == 12)
			{
				document.ConfigureSpatialGrid(Vector(350, 0, -250), Vector(800, 0, 1100));
				array<vector> anchor = {Vector(-1000, 0, 0), Vector(0, 0, 0)};
				document.AddAnchorSegment(anchor, 80, 0xffc54b4b, 7, false);
				baseline.AddAnchorSegment(anchor, 80, 0xffc54b4b, 7, false);
				anchor = {vector.Zero, Vector(1000, 0, 0)};
				document.AddAnchorSegment(anchor, 80, 0xffc54b4b, 7, true);
				baseline.AddAnchorSegment(anchor, 80, 0xffc54b4b, 7, true);
			}
		}
		path = {Vector(-1000000, 0, -1000000), Vector(1000000, 0, 1000000)};
		bool actualChanged = document.ErasePath(path, 2);
		bool expectedChanged = baseline.ErasePath(path, 2);
		Check(actualChanged == expectedChanged && SameDrawing(document, baseline), "long diagonal sweep uses populated cells and matches full scan");
		path = {vector.Zero};
		actualChanged = document.ErasePath(path, 10000);
		expectedChanged = baseline.ErasePath(path, 10000);
		Check(actualChanged == expectedChanged && SameDrawing(document, baseline) && document.m_Strokes.IsEmpty(), "large-radius sweep removes all indexed fragments");
		path.Clear();
		Check(!document.ErasePath(path, 2), "empty erase path is ignored");

		// A closed-map client loads a snapshot into its default grid, then applies
		// the same erase as an authority whose grid uses actual terrain bounds.
		document = MakeLine();
		document.ConfigureSpatialGrid(Vector(-100, 0, -100), Vector(1000, 0, 1500));
		LL_MapPaintReplica replica = new LL_MapPaintReplica();
		replica.AwaitSnapshot(1);
		replica.BeginSnapshot(1, 4);
		foreach (LL_MapPaintStroke stroke : document.m_Strokes)
			replica.SnapshotChunk(1, true, stroke.m_fWidth, stroke.m_iColor, stroke.m_Points, stroke.m_iOwner, stroke.m_bAnchorLine);
		path = {vector.Zero};
		replica.Receive(Operation(5, LL_EMapPaintOperation.ERASE, 2, path));
		document.ErasePath(path, 2);
		Check(replica.EndSnapshot(1) && replica.GetRevision() == 5 && SameDrawing(replica.m_Document, document), "closed-map replica indexes a snapshot before replaying buffered erasure");
		replica.m_Document.ConfigureSpatialGrid(Vector(50, 0, -50), Vector(700, 0, 900));
		path = {Vector(-20, 0, 0), Vector(20, 0, 0)};
		replica.Receive(Operation(6, LL_EMapPaintOperation.ERASE, 2, path));
		document.ErasePath(path, 2);
		Check(replica.GetRevision() == 6 && SameDrawing(replica.m_Document, document) && document.m_Strokes.IsEmpty(), "server and client erase identically after a map-grid reconfiguration");
	}

	protected void ValidateIncrementalIndex()
	{
		LL_MapPaintIndexProbe document = new LL_MapPaintIndexProbe();
		LL_MapPaintStroke growing = document.BeginStroke(Vector(-2000, 0, -2000), 2, 0xffc54b4b, 7);
		LL_MapPaintSpatialSegment dot = document.GetIndexedSegment(growing, 0);
		document.AppendPoint(growing, Vector(-1990, 0, -2000), 0);
		LL_MapPaintSpatialSegment firstEdge = document.GetIndexedSegment(growing, 1);
		Check(dot && dot.m_CellCoordinates.IsEmpty() && !document.GetIndexedSegment(growing, 0) && firstEdge, "appending the first edge removes the old dot from its cells");
		document.AppendPoint(growing, Vector(-1980, 0, -2000), 0);
		Check(document.GetIndexedSegment(growing, 1) == firstEdge && document.GetIndexedSegment(growing, 2), "appending a point preserves already indexed edges");
		array<vector> farPoints = {Vector(2000, 0, 2000), Vector(2010, 0, 2000)};
		LL_MapPaintStroke farStroke = document.AddStroke(farPoints, 2, 0xff00ff00, 8);
		LL_MapPaintSpatialSegment farEdge = document.GetIndexedSegment(farStroke, 1);
		array<vector> missPoints = {Vector(-2000, 0, -1990), Vector(-1980, 0, -1990)};
		LL_MapPaintStroke nearMiss = document.AddStroke(missPoints, 2, 0xff00ff00, 8);
		LL_MapPaintSpatialSegment missEdge = document.GetIndexedSegment(nearMiss, 1);
		array<vector> path = {Vector(-1995, 0, -2000)};
		Check(document.ErasePath(path, 1), "local cut changes the indexed drawing");
		Check(document.GetIndexedSegment(farStroke, 1) == farEdge && document.GetIndexedSegment(nearMiss, 1) == missEdge, "local cut preserves distant and non-intersecting candidate segment records");
		Check(!document.GetIndexedSegment(growing, 1) && firstEdge.m_CellCoordinates.IsEmpty(), "cut stroke entries are removed from all recorded cells");
		LL_MapPaintTooltipProbe tooltip = new LL_MapPaintTooltipProbe();
		Check(!tooltip.Pick(document, path[0]), "author hover cannot see removed geometry");
		Check(tooltip.Pick(document, Vector(-2000, 0, -2000)) != null, "author hover finds newly indexed surviving fragments");
		path = {vector.Zero};
		document.ErasePath(path, 10000);
		Check(document.GetIndexedStrokeCount() == 0 && document.GetSpatialSectorCount() == 0 && document.GetSpatialCellCount() == 0, "removing all geometry releases empty cells, sectors and stroke records");

		document = new LL_MapPaintIndexProbe();
		array<vector> bottomPoints = {Vector(-100, 0, 0), Vector(100, 0, 0)};
		array<vector> topPoints = {Vector(-20, 0, 0), Vector(20, 0, 0)};
		document.AddStroke(bottomPoints, 2, 0xffc54b4b, 7);
		LL_MapPaintStroke top = document.AddStroke(topPoints, 2, 0xff00ff00, 8);
		LL_MapPaintSpatialSegment topEdge = document.GetIndexedSegment(top, 1);
		int topId = top.m_iSpatialId;
		path = {Vector(-70, 0, 0)};
		document.ErasePath(path, 1);
		Check(document.GetIndexedSegment(top, 1) == topEdge && top.m_iSpatialId == topId, "splitting another stroke keeps an existing spatial ID and segment object");
		Check(tooltip.Pick(document, vector.Zero) == top, "new fragment IDs do not move an older stroke above a newer author");
		array<vector> newestPoints = {Vector(-10, 0, 0), Vector(10, 0, 0)};
		LL_MapPaintStroke newest = document.AddStroke(newestPoints, 2, 0xffffffff, 9);
		LL_MapPaintSpatialSegment newestEdge = document.GetIndexedSegment(newest, 1);
		Check(tooltip.Pick(document, vector.Zero) == newest, "new strokes remain above all existing fragments");

		LL_MapPaintDocument snapshot = new LL_MapPaintDocument();
		for (int i = document.m_Strokes.Count() - 1; i >= 0; i--)
		{
			LL_MapPaintStroke source = document.m_Strokes[i];
			snapshot.AddStroke(source.m_Points, source.m_fWidth, source.m_iColor, source.m_iOwner, source.m_bAnchorLine);
		}
		document.ReplaceFrom(snapshot);
		Check(SameDrawing(document, snapshot) && document.GetIndexedSegment(top, 1) == topEdge && document.GetIndexedSegment(newest, 1) == newestEdge, "snapshot reordering reuses unchanged strokes and their segment records");
		LL_MapPaintStroke hovered = tooltip.Pick(document, vector.Zero);
		Check(hovered && hovered.m_iOwner == 7, "author hover follows snapshot drawing order independently of spatial IDs");
		snapshot.AppendPoint(snapshot.m_Strokes[0], Vector(30, 0, 0), 0);
		document.ReplaceFrom(snapshot);
		LL_MapPaintStroke updated = document.m_Strokes[0];
		Check(document.GetIndexedSegment(top, 1) == topEdge && !document.GetIndexedSegment(newest, 1) && newestEdge.m_CellCoordinates.IsEmpty(), "snapshot change replaces only the modified stroke index");
		int savedCount = updated.m_Points.Count();
		snapshot.AppendPoint(snapshot.m_Strokes[0], Vector(40, 0, 0), 0);
		Check(updated.m_Points.Count() == savedCount, "snapshot replacement owns its geometry without sharing mutable source points");
		document.ConfigureSpatialGrid(vector.Zero, Vector(12800, 0, 12800));
		Check(document.GetIndexedSegment(top, 1) == topEdge, "unchanged grid bounds do not rebuild existing records");
		document.ConfigureSpatialGrid(Vector(-500, 0, -500), Vector(1000, 0, 1500));
		hovered = tooltip.Pick(document, vector.Zero);
		Check(top.m_iSpatialId == topId && hovered && hovered.m_iOwner == 7, "changing cell dimensions preserves spatial IDs and author stacking");

		// Same lookup bucket, different interior geometry: snapshots must match all
		// points before retaining records, including when the stroke order changes.
		document = new LL_MapPaintIndexProbe();
		array<vector> bentA = {Vector(-100, 0, 0), Vector(0, 0, 100), Vector(100, 0, 0)};
		array<vector> bentB = {Vector(-100, 0, 0), Vector(0, 0, -100), Vector(100, 0, 0)};
		LL_MapPaintStroke a = document.AddStroke(bentA, 2, 0xffc54b4b, 7);
		LL_MapPaintStroke b = document.AddStroke(bentB, 2, 0xffc54b4b, 7);
		LL_MapPaintSpatialSegment aEdge = document.GetIndexedSegment(a, 1);
		LL_MapPaintSpatialSegment bEdge = document.GetIndexedSegment(b, 1);
		snapshot = new LL_MapPaintDocument();
		snapshot.AddStroke(bentB, 2, 0xffc54b4b, 7);
		snapshot.AddStroke(bentA, 2, 0xffc54b4b, 7);
		document.ReplaceFrom(snapshot);
		Check(document.m_Strokes[0] == b && document.m_Strokes[1] == a && document.GetIndexedSegment(a, 1) == aEdge && document.GetIndexedSegment(b, 1) == bEdge, "snapshot buckets verify interior points and preserve matching index records");
		path = {Vector(0, 0, 100)};
		document.ErasePath(path, 1);
		Check(document.GetIndexedSegment(b, 1) == bEdge && !tooltip.Pick(document, path[0]), "hover and eraser share the updated grid after a snapshot and cut");
		snapshot = new LL_MapPaintDocument();
		document.ReplaceFrom(snapshot);
		Check(document.GetIndexedStrokeCount() == 0 && document.GetSpatialSectorCount() == 0, "empty snapshot removes obsolete records without a global rebuild");

		// A rejected edit must not partially remove entries before commit.
		document = new LL_MapPaintIndexProbe();
		array<vector> zigzag = {};
		for (int p = 0; p < LL_MapPaintDocument.MAX_POINTS; p++)
		{
			int x = -10;
			if (p % 2 == 1)
				x = 10;
			zigzag.Insert(Vector(x, 0, p * 0.001));
		}
		LL_MapPaintStroke budgetStroke = document.AddStroke(zigzag, 0.01, 0xffc54b4b);
		LL_MapPaintSpatialSegment budgetEdge = document.GetIndexedSegment(budgetStroke, 1);
		path = {Vector(0, 0, -100), Vector(0, 0, 100)};
		int version = document.GetVersion();
		Check(!document.ErasePath(path, 0.1) && document.GetVersion() == version && document.GetIndexedSegment(budgetStroke, 1) == budgetEdge && !budgetEdge.m_CellCoordinates.IsEmpty(), "point-budget rejection leaves all existing index entries intact");
	}

	protected LL_MapPaintOperation Operation(int revision, int kind, float size, array<vector> points)
	{
		LL_MapPaintOperation operation = new LL_MapPaintOperation();
		operation.m_iRevision = revision;
		operation.m_Kind = kind;
		operation.m_fSize = size;
		operation.m_iColor = 0xffc54b4b;
		operation.m_Points.Copy(points);
		return operation;
	}

	protected void ValidateMarkerReplay()
	{
		array<ref SCR_MapMarkerBase> active = {};
		array<ref SCR_MapMarkerBase> disabled = {};
		Check(SCR_MapMarkerManagerComponent.LL_GetStaticMarkerSnapshot(active, disabled).IsEmpty(), "empty marker lists have an empty restriction snapshot");
		SCR_MapMarkerBase first = new SCR_MapMarkerBase();
		first.SetMarkerID(10);
		SCR_MapMarkerBase second = new SCR_MapMarkerBase();
		second.SetMarkerID(20);
		active.Insert(first);
		disabled.Insert(second);
		Check(SCR_MapMarkerManagerComponent.LL_IsKnownStaticMarker(first, active, disabled), "listen-host replay of the same marker is rejected");
		SCR_MapMarkerBase replay = new SCR_MapMarkerBase();
		replay.SetMarkerID(10);
		Check(SCR_MapMarkerManagerComponent.LL_IsKnownStaticMarker(replay, active, disabled), "a deserialized replay is identified by its network ID");
		replay.SetMarkerID(20);
		Check(SCR_MapMarkerManagerComponent.LL_IsKnownStaticMarker(replay, active, disabled), "disabled markers are also protected from duplicate delivery");
		replay.SetMarkerID(30);
		Check(!SCR_MapMarkerManagerComponent.LL_IsKnownStaticMarker(replay, active, disabled), "a new network marker is accepted");
		SCR_MapMarkerBase local = new SCR_MapMarkerBase();
		active.Insert(local);
		replay.SetMarkerID(-1);
		Check(!SCR_MapMarkerManagerComponent.LL_IsKnownStaticMarker(replay, active, disabled), "distinct local markers can share the unassigned ID");
		Check(SCR_MapMarkerManagerComponent.LL_IsKnownStaticMarker(local, active, disabled), "the same local marker instance is not inserted twice");
		Check(!SCR_MapMarkerManagerComponent.LL_IsKnownStaticMarker(null, active, disabled), "null deliveries are ignored");

		array<ref SCR_MapMarkerBase> snapshot = SCR_MapMarkerManagerComponent.LL_GetStaticMarkerSnapshot(active, disabled);
		int visited;
		foreach (SCR_MapMarkerBase marker : snapshot)
		{
			// Exercise both relocation and removal while the restriction snapshot is traversed.
			active.RemoveItem(marker);
			disabled.Insert(marker);
			if (visited == 0)
			{
				active.Clear();
				disabled.Clear();
				active.Insert(replay);
			}
			visited++;
		}
		Check(visited == 3, "moving or removing live markers does not invalidate restriction iteration");
		Check(snapshot.Contains(first) && snapshot.Contains(second) && snapshot.Contains(local), "the restriction snapshot retains all original marker references");
		Check(!snapshot.Contains(replay), "markers arriving during a pass are left for the next snapshot");
		active.Clear();
		disabled.Clear();
		active.Insert(replay);
		Check(!SCR_MapMarkerManagerComponent.LL_IsKnownStaticMarker(first, active, disabled), "a marker removed on faction change can be delivered again");
	}

	protected void ValidateReplication()
	{
		array<vector> line = {Vector(-10, 0, 0), Vector(10, 0, 0)};
		array<vector> cut = {vector.Zero};
		Check(LL_MapPaintProtocol.Validate(0, 2, line), "valid network stroke accepted");
		Check(!LL_MapPaintProtocol.Validate(7, 2, line), "unknown network operation rejected");
		Check(!LL_MapPaintProtocol.Validate(0, -1, line), "invalid network width rejected");
		array<vector> invalid = {Vector(0, 1, 0)};
		Check(!LL_MapPaintProtocol.Validate(0, 2, invalid), "non-map coordinates rejected");
		array<vector> oversized = {};
		for (int p = 0; p <= LL_MapPaintProtocol.BATCH_POINTS; p++)
			oversized.Insert(Vector(p, 0, 0));
		Check(!LL_MapPaintProtocol.Validate(0, 2, oversized), "oversized network packet rejected");

		LL_MapPaintReplica replica = new LL_MapPaintReplica();
		replica.m_Document.ConfigureSpatialGrid(Vector(-100, 0, -100), Vector(1000, 0, 1000));
		replica.AwaitSnapshot(1);
		replica.BeginSnapshot(1, 0);
		Check(replica.EndSnapshot(1) && replica.IsReady(), "empty server snapshot enables editing");
		LL_MapPaintOperation draw = Operation(1, 0, 2, line);
		LL_MapPaintOperation erase = Operation(2, 1, 2, cut);
		replica.Receive(erase);
		Check(replica.GetRevision() == 0 && replica.m_Document.m_Strokes.IsEmpty(), "out-of-order erase waits for its preceding stroke");
		replica.Receive(draw);
		Check(replica.GetRevision() == 2 && replica.m_Document.m_Strokes.Count() == 2, "buffered operations replay in server order");
		replica.Receive(draw);
		replica.Receive(erase);
		Check(replica.m_Document.m_Strokes.Count() == 2 && replica.m_Document.GetPointCount() == 4, "repeated deltas are not applied twice");

		LL_MapPaintReplica lateJoiner = new LL_MapPaintReplica();
		lateJoiner.AwaitSnapshot(8);
		lateJoiner.BeginSnapshot(8, 2);
		foreach (LL_MapPaintStroke stroke : replica.m_Document.m_Strokes)
			lateJoiner.SnapshotChunk(8, true, stroke.m_fWidth, stroke.m_iColor, stroke.m_Points);
		array<vector> dot = {Vector(20, 0, 20)};
		lateJoiner.Receive(Operation(3, 0, 2, dot));
		Check(!lateJoiner.IsReady() && lateJoiner.m_Document.m_Strokes.IsEmpty(), "incomplete snapshots are not exposed to the UI");
		Check(lateJoiner.EndSnapshot(8) && lateJoiner.GetRevision() == 3, "late join applies edits received during its snapshot");
		Check(lateJoiner.m_Document.m_Strokes.Count() == 3 && lateJoiner.m_Document.GetPointCount() == 5, "late join preserves partial erasure and live additions");
		lateJoiner.BeginSnapshot(7, 0);
		Check(!lateJoiner.EndSnapshot(7) && lateJoiner.GetRevision() == 3, "obsolete snapshots cannot roll the map back");
		lateJoiner.Receive(Operation(5, 0, 2, dot));
		Check(lateJoiner.HasGap() && lateJoiner.GetRevision() == 3, "missing revisions are detected for snapshot recovery");
		lateJoiner.Receive(Operation(4, 0, 2, dot));
		Check(!lateJoiner.HasGap() && lateJoiner.GetRevision() == 5, "a delayed revision drains the reorder buffer");
		LL_MapPaintDocument visible = lateJoiner.m_Document;
		lateJoiner.AwaitSnapshot(9);
		lateJoiner.BeginSnapshot(9, 5);
		lateJoiner.SnapshotChunk(9, true, 2, 0xffc54b4b, oversized);
		Check(!lateJoiner.EndSnapshot(9) && visible == lateJoiner.m_Document && visible.GetPointCount() == 7, "malformed snapshot keeps the last valid shared drawing");
	}

	protected void ValidateCircleResolution()
	{
		array<float> radii = {1, 20, 50, 100, 200, 250, 1000};
		array<int> expectedSegments = {12, 12, 16, 28, 52, 64, 64};
		foreach (int index, float radius : radii)
		{
			array<vector> adaptive = {};
			Check(LL_MapPaintCircleGeometry.BuildPoints(vector.Zero, Vector(radius, 0, 0), adaptive) && adaptive.Count() == expectedSegments[index] + 1, "circle resolution follows its world-space radius");
			Check(adaptive[0] == adaptive[adaptive.Count() - 1] && Near(adaptive[expectedSegments[index] / 4][2], radius), "adaptive contours stay closed and reach their cardinal endpoints");
		}
		bool bounded = true;
		int previousSegments = 0;
		for (int sampleRadius = 1; sampleRadius <= 1000; sampleRadius++)
		{
			int segments = LL_MapPaintCircleGeometry.GetSegmentCount(sampleRadius);
			bounded &= segments >= previousSegments && segments >= 12 && segments <= 64 && segments % 4 == 0;
			previousSegments = segments;
		}
		Check(bounded, "circle resolution grows monotonically within its limits and preserves complete quadrants");
	}

	protected void ValidateCircles()
	{
		ValidateCircleResolution();
		array<vector> controls = {Vector(100, 0, 200), Vector(400, 0, 600)};
		Check(LL_MapPaintProtocol.Validate(LL_EMapPaintOperation.CIRCLE, 4, controls), "circle operation accepts a center and radius endpoint");
		array<vector> points = {};
		Check(LL_MapPaintCircleGeometry.BuildPoints(controls[0], controls[1], points) && points.Count() == LL_MapPaintCircleGeometry.MAX_SEGMENTS + 1, "large circle builds the maximum contour from its two network controls");
		Check(points[0] == points[points.Count() - 1], "circle contour closes without a floating-point seam");
		bool circular = true;
		foreach (vector point : points)
			circular &= point[1] == 0 && Near(vector.Distance(point, controls[0]), 500);
		Check(circular, "circle samples stay in map space at the requested radius");
		Check(Near(points[0][0], 600) && Near(points[16][2], 700) && Near(points[32][0], -400) && Near(points[48][2], -300), "circle reaches the four cardinal radius endpoints");
		array<vector> zeroRadius = {controls[0], controls[0]};
		Check(!LL_MapPaintProtocol.Validate(LL_EMapPaintOperation.CIRCLE, 4, zeroRadius), "clicking without dragging does not create a zero-radius circle");
		array<vector> incomplete = {controls[0]};
		Check(!LL_MapPaintProtocol.Validate(LL_EMapPaintOperation.CIRCLE, 4, incomplete), "circle operation rejects a missing radius endpoint");
		array<vector> overflow = {Vector(999990, 0, 0), Vector(999999, 0, 20)};
		Check(!LL_MapPaintProtocol.Validate(LL_EMapPaintOperation.CIRCLE, 4, overflow), "circle validation bounds the complete contour rather than just its controls");
		LL_MapPaintDocument document = new LL_MapPaintDocument();
		Check(LL_MapPaintProtocol.Apply(document, LL_EMapPaintOperation.CIRCLE, 4, 0x20332211, controls, 7) && document.GetPointCount() == points.Count(), "circle commits as one ordinary erasable stroke");
		LL_MapPaintStroke stroke = document.m_Strokes[0];
		Check(stroke.m_iOwner == 7 && stroke.m_iColor == 0xff332211 && !stroke.m_bAnchorLine, "circle preserves ownership and uses the common opaque brush style");

		LL_MapPaintOperation operation = new LL_MapPaintOperation();
		operation.m_iRevision = 1;
		operation.m_Kind = LL_EMapPaintOperation.CIRCLE;
		operation.m_iOwner = 7;
		operation.m_fSize = 4;
		operation.m_iColor = 0x20332211;
		operation.m_Points.Copy(controls);
		LL_MapPaintReplica replica = new LL_MapPaintReplica();
		replica.SetAuthority();
		replica.Receive(operation);
		Check(replica.GetRevision() == 1 && SameDrawing(document, replica.m_Document), "live circle replication reconstructs the authoritative contour from two controls");
		LL_MapPaintReplica lateJoiner = new LL_MapPaintReplica();
		lateJoiner.AwaitSnapshot(1);
		lateJoiner.BeginSnapshot(1, 1);
		for (int start = 0; start < points.Count(); start += LL_MapPaintProtocol.BATCH_POINTS)
		{
			array<vector> chunk = {};
			for (int i = start; i < Math.Min(start + LL_MapPaintProtocol.BATCH_POINTS, points.Count()); i++)
				chunk.Insert(points[i]);
			lateJoiner.SnapshotChunk(1, start == 0, 4, stroke.m_iColor, chunk, 7);
		}
		Check(lateJoiner.EndSnapshot(1) && SameDrawing(document, lateJoiner.m_Document), "late join snapshots preserve a circle across multiple point chunks");
		Check(!document.Erase(controls[0], controls[0], 2), "circle has an empty interior that the eraser does not hit");
		array<int> erasedOwners = {};
		array<vector> cut = {points[0]};
		Check(document.ErasePath(cut, 2, erasedOwners) && erasedOwners.Count() == 1 && erasedOwners[0] == 7, "eraser cuts the circle contour and reports its author");

		LL_MapPaintDocument full = new LL_MapPaintDocument();
		array<vector> filler = {};
		for (int i = 0; i < LL_MapPaintDocument.MAX_POINTS - points.Count() + 1; i++)
			filler.Insert(Vector(i, 0, -1000));
		full.AddStroke(filler, 4, 0xff000000);
		int oldCount = full.GetPointCount();
		Check(!LL_MapPaintProtocol.Apply(full, LL_EMapPaintOperation.CIRCLE, 4, 0xff000000, controls) && full.GetPointCount() == oldCount, "point-budget rejection never leaves a partial circle");
		array<vector> smallControls = {Vector(100, 0, 200), Vector(110, 0, 200)};
		operation.m_Points.Copy(smallControls);
		Check(LL_MapPaintProtocol.Apply(full, LL_EMapPaintOperation.CIRCLE, 4, 0xff000000, smallControls) && full.GetPointCount() == oldCount + 13, "a small circle uses its actual point budget when a large circle no longer fits");
		LL_MapPaintDocument smallDocument = new LL_MapPaintDocument();
		LL_MapPaintProtocol.Apply(smallDocument, LL_EMapPaintOperation.CIRCLE, 4, stroke.m_iColor, smallControls, 7);
		LL_MapPaintReplica smallReplica = new LL_MapPaintReplica();
		smallReplica.SetAuthority();
		smallReplica.Receive(operation);
		Check(SameDrawing(smallDocument, smallReplica.m_Document), "replication reconstructs the same minimum-resolution circle");
		Check(smallDocument.Erase(Vector(110, 0, 200), Vector(110, 0, 200), 2), "minimum-resolution circles use the ordinary spatial eraser index");
		LL_MapPaintHistory history = new LL_MapPaintHistory();
		history.Record(7, "Circle author", LL_EMapPaintOperation.LINE, 0);
		Check(history.Record(7, "Circle author", LL_EMapPaintOperation.CIRCLE, 1) && !history.Record(7, "Circle author", LL_EMapPaintOperation.CIRCLE, 9999), "circle history uses its own tool and the common ten-second interval");
		Check(history.Snapshot(10000)[0].GetText() == WidgetManager.Translate("#LL-MapPaint_HistoryCircle", "Circle author"), "circle history identifies the author and circle action");
	}

	protected void ValidateHistory()
	{
		LL_MapPaintHistory history = new LL_MapPaintHistory();
		Check(history.Record(1, "Alpha", LL_EMapPaintOperation.BRUSH, 0), "history records the first change immediately");
		Check(!history.Record(1, "Alpha", LL_EMapPaintOperation.BRUSH, 9999), "same tool is grouped for ten seconds");
		Check(history.Record(2, "Bravo", LL_EMapPaintOperation.BRUSH, 9999), "history grouping is independent for each player");
		Check(history.Record(1, "Alpha", LL_EMapPaintOperation.BRUSH, 10000), "continuous use creates a record at ten seconds without a sliding interval");
		Check(history.Record(1, "Alpha", LL_EMapPaintOperation.LINE, 10001), "using a different tool starts a record immediately");
		Check(!history.Record(1, "Alpha", LL_EMapPaintOperation.LINE, 20000), "tool switch restarts the ten-second interval");
		Check(history.Record(1, "Alpha", LL_EMapPaintOperation.LINE, 20001), "new tool records again after its full interval");
		Check(history.Record(1, "Alpha", LL_EMapPaintOperation.BRUSH, 20002), "switching back to a previous tool starts another record");
		Check(history.Record(1, "Alpha", LL_EMapPaintOperation.ANCHOR, 20003), "anchor tool is distinct from the brush and straight line");
		Check(!history.Record(1, "Alpha", LL_EMapPaintOperation.ANCHOR_CONTINUE, 20004), "anchor continuation belongs to the same tool");
		Check(history.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 20005), "eraser starts its own action record");
		array<ref LL_MapPaintHistoryEntry> firstOpen = history.Snapshot(20005);
		Check(firstOpen.Count() == 8 && firstOpen[0].m_iTool == LL_EMapPaintMode.ERASER && firstOpen[7].m_iCreatedAt == 0, "history snapshots show newest changes first");
		Check(firstOpen[0].m_sAuthor == "Alpha" && firstOpen[6].m_sAuthor == "Bravo", "history retains the author on each action");
		history.Record(3, "Charlie", LL_EMapPaintOperation.BRUSH, 20006);
		Check(firstOpen.Count() == 8 && firstOpen[0].m_sAuthor == "Alpha", "open history snapshot does not receive new actions");
		array<ref LL_MapPaintHistoryEntry> reopened = history.Snapshot(20006);
		Check(reopened.Count() == 9 && reopened[0].m_sAuthor == "Charlie", "reopening fetches new actions");
		history.Prune(300000);
		array<ref LL_MapPaintHistoryEntry> pruned = history.Snapshot(300000);
		Check(pruned.Count() == 8 && pruned[7].m_iCreatedAt == 9999 && pruned[0].m_sAuthor == "Charlie", "records expire at five minutes while newer records keep chronological order");
		Check(firstOpen.Count() == 8 && firstOpen[7].m_iCreatedAt == 0, "pruning the server history does not mutate an open snapshot");
		history.Prune(320006);
		Check(history.IsEmpty(), "history expires fully without further drawing");
		Check(history.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 320007), "history can record again after all previous actions expire");

		LL_MapPaintHistory otherFaction = new LL_MapPaintHistory();
		otherFaction.Record(1, "Alpha", LL_EMapPaintOperation.LINE, 320008);
		array<ref LL_MapPaintHistoryEntry> firstFaction = history.Snapshot(320008);
		array<ref LL_MapPaintHistoryEntry> secondFaction = otherFaction.Snapshot(320008);
		Check(firstFaction.Count() == 1 && secondFaction.Count() == 1 && firstFaction[0].m_iTool != secondFaction[0].m_iTool, "separate faction histories do not share records or grouping");

		array<vector> line = {Vector(0, 0, 0), Vector(10, 0, 0)};
		Check(LL_MapPaintProtocol.Validate(LL_EMapPaintOperation.LINE, 2, line), "straight-line history operation accepts valid endpoints");
		LL_MapPaintDocument document = new LL_MapPaintDocument();
		Check(LL_MapPaintProtocol.Apply(document, LL_EMapPaintOperation.LINE, 2, 0xff000000, line, 1) && document.GetPointCount() == 2, "straight-line operation preserves drawing geometry");
		line.Remove(1);
		Check(!LL_MapPaintProtocol.Validate(LL_EMapPaintOperation.LINE, 2, line), "straight-line operation rejects incomplete endpoints");
	}

	protected void ValidateEraseHistory()
	{
		LL_MapPaintDocument document = new LL_MapPaintDocument();
		array<vector> firstLine = {Vector(-10, 0, 0), Vector(10, 0, 0)};
		array<vector> secondLine = {Vector(-10, 0, 2), Vector(10, 0, 2)};
		document.AddStroke(firstLine, 2, 0xff000000, 11);
		document.AddStroke(secondLine, 2, 0xff000000, 12);
		document.AddStroke(firstLine, 2, 0xff000000, 11);
		array<vector> distantDot = {Vector(0, 0, 100)};
		document.AddStroke(distantDot, 2, 0xff000000, 13);
		array<int> erasedOwners = {};
		array<vector> cut = {vector.Zero};
		Check(LL_MapPaintProtocol.Apply(document, LL_EMapPaintOperation.ERASE, 2, 0xff000000, cut, 1, erasedOwners), "eraser with author reporting applies partial cuts");
		Check(erasedOwners.Count() == 2 && erasedOwners.Contains(11) && erasedOwners.Contains(12) && !erasedOwners.Contains(13), "eraser reports only affected owners and deduplicates multiple drawings by one owner");
		array<vector> miss = {Vector(900, 0, 900)};
		Check(!document.ErasePath(miss, 2, erasedOwners) && erasedOwners.IsEmpty(), "no-op erasing clears stale affected-owner data");
		Check(document.ErasePath(distantDot, 2, erasedOwners) && erasedOwners.Count() == 1 && erasedOwners[0] == 13, "fully removed drawings also report their owner");

		LL_MapPaintHistory history = new LL_MapPaintHistory();
		array<string> firstAuthors = {"Bravo"};
		history.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 0, firstAuthors);
		array<ref LL_MapPaintHistoryEntry> firstOpen = history.Snapshot(0);
		array<string> secondAuthors = {"Charlie"};
		Check(history.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 5000, secondAuthors), "erasing a different player's drawing creates a record before ten seconds pass");
		array<ref LL_MapPaintHistoryEntry> reopened = history.Snapshot(5000);
		Check(reopened.Count() == 2 && reopened[0].GetText() == WidgetManager.Translate("#LL-MapPaint_HistoryEraseAuthor", "Alpha", "Charlie") && reopened[1].GetText() == WidgetManager.Translate("#LL-MapPaint_HistoryEraseAuthor", "Alpha", "Bravo"), "each eraser target has its own newest-first record");
		Check(!history.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 14999, secondAuthors), "changing eraser targets restarts the full ten-second interval");
		Check(history.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 15000, secondAuthors), "same eraser target records again at the restarted interval boundary");
		Check(history.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 15001, firstAuthors), "switching back to the previous eraser target starts another record immediately");
		Check(history.Record(2, "Other eraser", LL_EMapPaintOperation.ERASE, 15002, firstAuthors), "different eraser players have independent target groups");
		Check(!history.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 25000, firstAuthors) && history.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 25001, firstAuthors), "another player's erasing does not reset the original player's interval");
		Check(history.Record(1, "Alpha", LL_EMapPaintOperation.BRUSH, 25002) && history.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 25003, firstAuthors), "switching away from and back to the eraser creates a fresh record");
		Check(firstOpen.Count() == 1 && firstOpen[0].GetText() == WidgetManager.Translate("#LL-MapPaint_HistoryEraseAuthor", "Alpha", "Bravo"), "new eraser target records do not mutate an open snapshot");

		LL_MapPaintHistory simultaneous = new LL_MapPaintHistory();
		array<string> bothAuthors = {"Bravo", "Charlie", "Bravo"};
		Check(simultaneous.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 0, bothAuthors), "one eraser operation can affect multiple owners");
		reopened = simultaneous.Snapshot(0);
		Check(reopened.Count() == 2 && reopened[0].m_sErasedAuthor == "Charlie" && reopened[1].m_sErasedAuthor == "Bravo", "simultaneous erasing creates one distinct record per owner without duplicates");
		Check(!simultaneous.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 1000, bothAuthors), "repeated multi-owner eraser packets do not spam alternating records");
		array<string> reversedAuthors = {"Charlie", "Bravo"};
		Check(!simultaneous.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 2000, reversedAuthors), "multi-owner grouping does not depend on geometry query order");
		Check(!simultaneous.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 3000, secondAuthors) && simultaneous.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 4000, firstAuthors), "after continuing on one simultaneous target, switching to the other records immediately");
		Check(!simultaneous.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 13999, firstAuthors) && simultaneous.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 14000, firstAuthors), "switching from simultaneous targets restarts the interval");

		LL_MapPaintHistory expanding = new LL_MapPaintHistory();
		expanding.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 0, firstAuthors);
		expanding.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 1000, bothAuthors);
		reopened = expanding.Snapshot(1000);
		Check(reopened.Count() == 2 && reopened[0].GetText() == WidgetManager.Translate("#LL-MapPaint_HistoryEraseAuthor", "Alpha", "Charlie"), "adding an eraser target records the new owner without repeating the current owner");
		Check(!expanding.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 10999, bothAuthors) && expanding.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 11000, bothAuthors), "adding an eraser target resets the shared ten-second countdown");

		LL_MapPaintHistory sameNames = new LL_MapPaintHistory();
		array<string> sameNickname = {"Same nickname"};
		array<int> firstOwner = {11};
		array<int> secondOwner = {12};
		sameNames.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 0, sameNickname, firstOwner);
		Check(sameNames.Record(1, "Alpha", LL_EMapPaintOperation.ERASE, 1, sameNickname, secondOwner) && sameNames.Snapshot(1).Count() == 2, "different drawing owners with identical nicknames still create separate records");
	}

	protected void ValidateHistoryBatches()
	{
		LL_MapPaintHistorySnapshot snapshot = new LL_MapPaintHistorySnapshot();
		array<string> authors = {};
		array<int> tools = {};
		array<string> erasedAuthors = {};
		for (int i = 0; i < LL_MapPaintHistorySnapshot.BATCH_SIZE; i++)
		{
			authors.Insert(string.Format("Author %1", i));
			tools.Insert(LL_EMapPaintMode.ERASER);
			erasedAuthors.Insert(string.Format("Drawing owner %1", i));
		}
		Check(snapshot.AppendBatch(authors, tools, erasedAuthors) && snapshot.m_Entries.Count() == 50 && !snapshot.m_bReady, "a full history batch stays pending until the snapshot ends");
		Check(snapshot.m_Entries[49].GetText() == WidgetManager.Translate("#LL-MapPaint_HistoryEraseAuthor", "Author 49", "Drawing owner 49"), "history batch decoding keeps each action and erased author together");
		array<string> lastAuthor = {"Last author"};
		array<int> lastTool = {LL_EMapPaintMode.LINE};
		array<string> lastErasedAuthor = {string.Empty};
		Check(snapshot.AppendBatch(lastAuthor, lastTool, lastErasedAuthor) && snapshot.m_Entries.Count() == 51 && snapshot.m_Entries[50].GetText() == WidgetManager.Translate("#LL-MapPaint_HistoryLine", "Last author"), "a partial history batch appends in order after a full batch");
		snapshot.m_bReady = true;
		Check(!snapshot.AppendBatch(lastAuthor, lastTool, lastErasedAuthor) && snapshot.m_Entries.Count() == 51, "a completed history snapshot rejects further batches");

		snapshot = new LL_MapPaintHistorySnapshot();
		snapshot.AppendBatch(authors, tools, erasedAuthors);
		Check(!snapshot.AppendBatch(authors, lastTool, erasedAuthors) && !snapshot.m_bAvailable && snapshot.m_Entries.IsEmpty(), "mismatched history arrays discard the incomplete snapshot");
		Check(!snapshot.AppendBatch(lastAuthor, lastTool, lastErasedAuthor) && snapshot.m_Entries.IsEmpty(), "a failed history snapshot cannot resume with partial data");
		authors.Insert("Too many authors");
		tools.Insert(LL_EMapPaintMode.BRUSH);
		erasedAuthors.Insert(string.Empty);
		snapshot = new LL_MapPaintHistorySnapshot();
		Check(!snapshot.AppendBatch(authors, tools, erasedAuthors) && !snapshot.m_bAvailable && snapshot.m_Entries.IsEmpty(), "history batches cannot exceed 50 records");
	}

	protected void ValidateHistoryWindow()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		LL_MapPaintHistory history = new LL_MapPaintHistory();
		for (int i = 0; i < 20; i++)
			history.Record(i, string.Format("Player %1", i), LL_EMapPaintOperation.BRUSH, i);
		LL_MapPaintHistoryPanel panel = new LL_MapPaintHistoryPanel();
		Widget root = panel.LL_TestShowEntries(workspace, history.Snapshot(12));
		Check(root && panel.IsOpen(), "history opens in a separate workspace window");
		if (!root)
			return;
		root.Update();
		ScrollLayoutWidget scroll = ScrollLayoutWidget.Cast(root.FindAnyWidget("LL_MapPaintHistoryScroll"));
		TextWidget firstEntry = TextWidget.Cast(root.FindAnyWidget("LL_HistoryEntry0"));
		Check(firstEntry && firstEntry.GetText() == WidgetManager.Translate("#LL-MapPaint_HistoryBrush", "Player 19") && firstEntry.GetNumLines() == 1, "short history entries show author then action on one line");
		Check(scroll != null, "history contains a native scroll layout");
		if (scroll)
		{
			scroll.SetSliderPos(0, 1);
			float x, y;
			scroll.GetSliderPosPixels(x, y);
			Check(y > 0 && x == 0, "history list scrolls vertically when rows exceed its height");
			panel.Scroll(1);
			float afterX, afterY;
			scroll.GetSliderPosPixels(afterX, afterY);
			Check(afterY < y && afterX == 0, "history mouse wheel moves its own vertical scroll");
		}
		array<string> erasedAuthors = {"Bravo with a very long nickname that needs several lines to display in full without hiding the drawing owner's name"};
		history.Record(20, "New player with a very long nickname", LL_EMapPaintOperation.ERASE, 20, erasedAuthors);
		Check(!root.FindAnyWidget("LL_HistoryEntry20") && firstEntry && firstEntry.GetText() == WidgetManager.Translate("#LL-MapPaint_HistoryBrush", "Player 19"), "visible history rows stay frozen when the live log changes");
		panel.Close();
		Check(!panel.IsOpen(), "history close removes its workspace window");
		root = panel.LL_TestShowEntries(workspace, history.Snapshot(20));
		root.Update();
		firstEntry = TextWidget.Cast(root.FindAnyWidget("LL_HistoryEntry0"));
		TextWidget nextEntry = TextWidget.Cast(root.FindAnyWidget("LL_HistoryEntry1"));
		Check(firstEntry && firstEntry.GetText().IndexOf(WidgetManager.Translate("#LL-MapPaint_HistoryEraseAuthor", "New player with a very long nickname", "Bravo")) == 0 && root.FindAnyWidget("LL_HistoryEntry20"), "reopening history includes the eraser action and affected authors");
		Check(firstEntry && firstEntry.GetNumLines() > 1, "long history entries wrap onto multiple lines");
		if (firstEntry && nextEntry)
		{
			float textWidth, textHeight;
			firstEntry.GetTextSize(textWidth, textHeight);
			Check(FrameSlot.GetSizeY(firstEntry) >= workspace.DPIUnscale(textHeight) && FrameSlot.GetPosY(nextEntry) >= FrameSlot.GetPosY(firstEntry) + FrameSlot.GetSizeY(firstEntry), "wrapped history entries have enough height and do not overlap the following row");
		}
		panel.Close();
	}

	protected void ValidateMapWindowPlacement()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		LL_MapPaintHistoryPanel panel = new LL_MapPaintHistoryPanel();
		array<ref LL_MapPaintHistoryEntry> entries = {};
		Widget window = panel.LL_TestShowEntries(workspace, entries);
		Widget menu = LL_MapPaintWidgets.CreateMapPanelRoot(workspace, "LL_TestMapMenu", 0, 0, 240, 40);
		Widget button;
		if (menu)
			button = LL_MapPaintWidgets.CreatePanelWidget(workspace, menu, WidgetType.ButtonWidgetTypeID, "LL_TestMapMenuButton", 0, 0, 0, 40, 40, 0xffffffff);
		Check(window && menu && button, "history placement test creates native menu and window widgets");
		if (window && menu && button)
		{
			vector screen = LL_MapPaintWidgets.GetScreenSizeInUIUnits(workspace);
			float margin = LL_MapPaintSettings.PAINT_PANEL_MARGIN;
			float gap = LL_MapPaintSettings.PAINT_PANEL_GAP;
			array<vector> positions = {Vector(screen[0] * 0.5, margin, 0), Vector(screen[0] * 0.5, screen[1] - 40 - margin, 0), Vector(margin, screen[1] * 0.25, 0), Vector(screen[0] - 40 - margin, screen[1] * 0.25, 0)};
			array<string> sides = {"below", "above", "right", "left"};
			foreach (int index, vector menuPosition : positions)
			{
				bool vertical = index >= 2;
				FrameSlot.SetAnchorMin(menu, 0, 0);
				FrameSlot.SetAnchorMax(menu, 0, 0);
				FrameSlot.SetPos(menu, menuPosition[0], menuPosition[1]);
				if (vertical)
					FrameSlot.SetSize(menu, 40, 240);
				else
					FrameSlot.SetSize(menu, 240, 40);
				menu.Update();
				LL_MapPaintWidgets.PositionPanelBesideMenu(workspace, window, button);
				vector position = FrameSlot.GetPos(window);
				float expected;
				float actual;
				switch (index)
				{
					case 0: actual = position[1]; expected = menuPosition[1] + 40 + gap; break;
					case 1: actual = position[1]; expected = menuPosition[1] - FrameSlot.GetSizeY(window) - gap; break;
					case 2: actual = position[0]; expected = menuPosition[0] + 40 + gap; break;
					case 3: actual = position[0]; expected = menuPosition[0] - FrameSlot.GetSizeX(window) - gap; break;
				}
				Check(LL_MapPaintWidgets.IsMapMenuVertical(button) == vertical && Near(actual, expected), "history opens on the roomier side of the menu: " + sides[index]);
				Check(position[0] >= margin - 0.01 && position[1] >= margin - 0.01 && position[0] + FrameSlot.GetSizeX(window) <= screen[0] - margin + 0.01 && position[1] + FrameSlot.GetSizeY(window) <= screen[1] - margin + 0.01, "history placement stays within the screen margins");
			}
		}
		if (menu)
			menu.RemoveFromHierarchy();
		panel.Close();
	}

	override void Run()
	{
		Validate();
	}

	override void RunCommandline()
	{
		Validate();
		Workbench.Exit(m_iFailures);
	}
}

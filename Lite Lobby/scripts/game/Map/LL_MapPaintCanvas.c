//! Canvas command helpers shared by stroke renderers.
class LL_MapPaintCanvas
{

	//! Bound each command to the engine limit and share the endpoint at each seam.
	static void AddLines(array<float> vertices, float width, int color, array<ref CanvasWidgetCommand> commands)
	{
		int first;
		int count = vertices.Count();
		while (first + 3 < count)
		{
			LineDrawCommand line = new LineDrawCommand();
			line.m_iColor = color;
			line.m_fWidth = width;
			line.m_Vertices = {};
			int end = Math.Min(first + SCR_MapConstants.CANVAS_COMMAND_VERTICES_LIMIT, count);
			for (int i = first; i < end; i++)
				line.m_Vertices.Insert(vertices[i]);
			commands.Insert(line);
			first = end - 2;
		}
	}
}

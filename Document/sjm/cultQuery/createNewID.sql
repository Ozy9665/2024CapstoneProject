SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
-- =============================================
CREATE PROCEDURE createNewID @Param1 VARCHAR(50), @Param2 VARCHAR(50)
	-- Add the parameters for the stored procedure here
AS
BEGIN
	SET NOCOUNT ON;

	INSERT INTO dbo.UserData(user_id, password)
    VALUES (@Param1, @Param2);
END
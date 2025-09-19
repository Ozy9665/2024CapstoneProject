-- ================================================
-- Template generated from Template Explorer using:
-- Create Procedure (New Menu).SQL
--
-- Use the Specify Values for Template Parameters 
-- command (Ctrl-Shift-M) to fill in the parameter 
-- values below.
--
-- This block of comments will not be included in
-- the definition of the procedure.
-- ================================================
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
-- =============================================
-- Author:		<Author,,Name>
-- Create date: <Create Date,,>
-- Description:	<Description,,>
-- =============================================
CREATE PROCEDURE isValidID  @Param1 VARCHAR(50)
	-- Add the parameters for the stored procedure here
AS
BEGIN
	SET NOCOUNT ON;
SELECT CASE WHEN EXISTS (SELECT 1 FROM dbo.UserData WHERE user_id = @Param1) THEN 1 ELSE 0 END AS IsValid;
END
GO

Academic Database Analysis and Optimization

Overview : 

This project focuses on designing, querying, and optimizing a relational academic database using MySQL.
The database contains information about students, professors, courses, departments, enrollments, and teaching assignments.

The goal of the project is to extract meaningful insights using SQL and improve query performance using indexing and execution plan analysis.

Objectives : 

* Design and work with a structured relational database
* Write advanced SQL queries for data analysis
* Extract insights related to academics such as:
    * Student enrollment patterns
    * Instructor workload distribution
    * Department-wise academic statistics
* Improve query performance using indexing techniques
* Analyze query execution using EXPLAIN plans

Database Schema  : 

The database typically includes the following relations:

* Student (student details and department mapping)
* Professor (faculty details)
* Department (department information)
* Course (course details)
* Enrollment (student-course mapping)
* Teaching (faculty-course mapping)
* Prerequisite (course dependencies)

Key SQL Features Used : 

* SELECT, WHERE, ORDER BY
* JOIN operations (INNER JOIN, LEFT JOIN)
* Aggregate functions (COUNT, AVG, SUM)
* GROUP BY and HAVING clauses
* Subqueries
* Index creation and usage
* Query plan analysis using EXPLAIN

Key Queries Implemented : 

* Student enrollment analysis across courses
* Instructor workload calculation (courses per faculty)
* Department-wise student and course distribution
* Popular courses based on enrollment count
* Students with high course loads


Query Optimization : 

To improve performance:

* Indexes were created on frequently queried attributes (e.g., course_id, roll_no, dept_id)
* Query execution plans were analyzed using EXPLAIN
* Comparison was made between queries before and after indexing
* Reduced number of rows scanned in optimized queries

ex: 

-- Before Index
EXPLAIN SELECT * FROM Enrollment WHERE course_id = 'CS101';

-- Creating Index
CREATE INDEX idx_course_id ON Enrollment(course_id);

-- After Index
EXPLAIN SELECT * FROM Enrollment WHERE course_id = 'CS101';

Outcomes : 

* Successfully extracted insights from academic data using SQL
* Identified patterns in student enrollment and faculty workload
* Improved query performance through indexing
* Gained hands-on experience with relational database optimization


Tools Used : 

* MySQL
* SQL
* EXPLAIN Query Analyzer


How to Run : 

1. Import the database schema into MySQL
2. Load sample data (if provided)
3. Run queries from queries.sql
4. Run optimization queries from optimization.sql
